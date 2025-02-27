/*
Copyright (c) 2014, Nghia Ho
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
 
#include <opencv2/opencv.hpp>
#include <iostream>
#include <cassert>
#include <cmath>
#include <fstream>
#include <chrono>
#include <opencv2/optflow.hpp>

using namespace std;
using namespace cv;

// This video stablisation smooths the global trajectory using a sliding average window

const int SMOOTHING_RADIUS = 50; // In frames. The larger the more stable the video, but less reactive to sudden panning
const int HORIZONTAL_BORDER_CROP = 70; // In pixels. Crops the border to reduce the black borders from stabilisation being too noticeable.

// 1. Get previous to current frame transformation (dx, dy, da) for all frames
// 2. Accumulate the transformations to get the image trajectory
// 3. Smooth out the trajectory using an averaging window
// 4. Generate new set of previous to current transform, such that the trajectory ends up being the same as the smoothed trajectory
// 5. Apply the new transformation to the video

struct TransformParam
{
    TransformParam() {}
    TransformParam(double _dx, double _dy, double _da) {
        dx = _dx;
        dy = _dy;
        da = _da;
    }

    double dx;
    double dy;
    double da; // angle
};

struct Trajectory
{
    Trajectory() {}
    Trajectory(double _x, double _y, double _a) {
        x = _x;
        y = _y;
        a = _a;
    }

    double x;
    double y;
    double a; // angle
};

int main(int argc, char **argv)
{
    if(argc < 3) {
        cout << "Usage: ./VideoStab [video.avi] [optical_flow_algorithm]" << endl;
        cout << "Optical Flow Algorithm: " << endl;
        cout << "1: LK Sparse" << endl;
        cout << "2: LK Dense" << endl;
        cout << "3: Farneback" << endl;
        return 0;
    }

    string optical_flow_algorithm = argv[2];
    double optical_flow_time = 0.0, generating_time= 0.0, transform_time = 0.0, trajectory_time = 0.0, smoothing_time = 0.0;
    
    // For further analysis
    ofstream out_transform("prev_to_cur_transformation.txt");
    ofstream out_trajectory("trajectory.txt");
    ofstream out_smoothed_trajectory("smoothed_trajectory.txt");
    ofstream out_new_transform("new_prev_to_cur_transformation.txt");


    VideoCapture cap(argv[1]);
    assert(cap.isOpened());
    if (optical_flow_algorithm == "1") {
        cout << "Ejecutando LK Sparse Optical Flow" << endl;
    } else if (optical_flow_algorithm == "2") {
        cout << "Ejecutando LK Dense Optical Flow" << endl;
    } else if (optical_flow_algorithm == "3") {
        cout << "Ejecutando Farneback Optical Flow" << endl;
    }

    auto start = std::chrono::high_resolution_clock::now();
    Mat cur, cur_grey;
    Mat prev, prev_grey;

    cap >> prev;
    cap >> prev;
    cvtColor(prev, prev_grey, COLOR_BGR2GRAY);

    // Vector para almacenar las transformaciones
    vector<TransformParam> prev_to_cur_transform;

    int k = 1;
    int max_frames = cap.get(cv::CAP_PROP_FRAME_COUNT) > 500 ? 500 : cap.get(cv::CAP_PROP_FRAME_COUNT);
    Mat last_T;


    auto start_optical_flow = chrono::high_resolution_clock::now();
    while (true) {
        cap >> cur;

        if (cur.data == NULL || k > max_frames) {
            break;
        }

        cvtColor(cur, cur_grey, COLOR_BGR2GRAY);

        if (optical_flow_algorithm == "1") {
            // Lucas-Kanade Disperso (Sparse)
            vector<Point2f> prev_corner, cur_corner;
            vector<Point2f> prev_corner2, cur_corner2;
            vector<uchar> status;
            vector<float> err;
            goodFeaturesToTrack(prev_grey, prev_corner, 200, 0.01, 30);
            calcOpticalFlowPyrLK(prev_grey, cur_grey, prev_corner, cur_corner, status, err);

            // Filtrado de coincidencias válidas
            for (size_t i = 0; i < status.size(); i++) {
                if (status[i]) {
                    prev_corner2.push_back(prev_corner[i]);
                    cur_corner2.push_back(cur_corner[i]);
                }
            }

            Mat T;
            try {
                T = estimateRigidTransform(prev_corner2, cur_corner2, false); // false = rigid transform, no scaling/shearing
            } catch(...) {
                continue;
            }

            if (T.data == NULL) {
                last_T.copyTo(T);
            }
            T.copyTo(last_T);

            double dx = T.at<double>(0, 2);
            double dy = T.at<double>(1, 2);
            double da = atan2(T.at<double>(1, 0), T.at<double>(0, 0));

            prev_to_cur_transform.push_back(TransformParam(dx, dy, da));
            out_transform << k << " " << dx << " " << dy << " " << da << endl;

        }   else if (optical_flow_algorithm == "2") {
            // Lucas-Kanade Denso (Dense)
            Mat flow;
            optflow::calcOpticalFlowSparseToDense(prev_grey, cur_grey, flow,  8, 128, 0.05f, true, 500.0f, 1.5f);

            // Cálculo de desplazamientos promedio
            double dx = 0.0, dy = 0.0, da = 0.0;
            int count = 0;

            for (int y = 0; y < flow.rows; y++) {
                for (int x = 0; x < flow.cols; x++) {
                    Point2f flow_at_point = flow.at<Point2f>(y, x);
                    dx += flow_at_point.x;
                    dy += flow_at_point.y;
                    count++;
                }
            }

            dx /= count;
            dy /= count;
            prev_to_cur_transform.push_back(TransformParam(dx, dy, 0));
            out_transform << k << " " << dx << " " << dy << " " << da << endl;
        }
        else if (optical_flow_algorithm == "3") {
            // Farneback
            Mat flow;
            calcOpticalFlowFarneback(prev_grey, cur_grey, flow, 0.3, 2, 10, 5, 7, 1.5, 0);

            // Cálculo de desplazamientos promedio
            double dx = 0.0, dy = 0.0, da = 0.0;
            int count = 0;

            for (int y = 0; y < flow.rows; y++) {
                for (int x = 0; x < flow.cols; x++) {
                    Point2f flow_at_point = flow.at<Point2f>(y, x);
                    dx += flow_at_point.x;
                    dy += flow_at_point.y;
                    count++;
                }
            }

            dx /= count;
            dy /= count;
            prev_to_cur_transform.push_back(TransformParam(dx, dy, 0));
            out_transform << k << " " << dx << " " << dy << " " << da << endl;
        }

        // Actualiza prev para el siguiente frame
        cur.copyTo(prev);
        cur_grey.copyTo(prev_grey);
        
        k++;
    }
    auto end_optical_flow = chrono::high_resolution_clock::now();
    optical_flow_time += chrono::duration<double>(end_optical_flow - start_optical_flow).count();


// Step 2 - Accumulate the transformations to get the image trajectory

    // Accumulated frame to frame transform
    double a = 0;
    double x = 0;
    double y = 0;
    auto start_trajectory = chrono::high_resolution_clock::now();
    vector <Trajectory> trajectory; // trajectory at all frames

    for(size_t i=0; i < prev_to_cur_transform.size(); i++) {
        x += prev_to_cur_transform[i].dx;
        y += prev_to_cur_transform[i].dy;
        a += prev_to_cur_transform[i].da;

        trajectory.push_back(Trajectory(x,y,a));

        out_trajectory << (i+1) << " " << x << " " << y << " " << a << endl;
    }
    auto end_trajectory = chrono::high_resolution_clock::now();
    trajectory_time += chrono::duration<double>(end_trajectory - start_trajectory).count();
    // Step 3 - Smooth out the trajectory using an averaging window
    vector <Trajectory> smoothed_trajectory; // trajectory at all frames
    auto start_smoothing = chrono::high_resolution_clock::now();
    for(size_t i=0; i < trajectory.size(); i++) {
        double sum_x = 0;
        double sum_y = 0;
        double sum_a = 0;
        int count = 0;

        for(int j=-SMOOTHING_RADIUS; j <= SMOOTHING_RADIUS; j++) {
            if(i+j >= 0 && i+j < trajectory.size()) {
                sum_x += trajectory[i+j].x;
                sum_y += trajectory[i+j].y;
                sum_a += trajectory[i+j].a;

                count++;
            }
        }

        double avg_a = sum_a / count;
        double avg_x = sum_x / count;
        double avg_y = sum_y / count;

        smoothed_trajectory.push_back(Trajectory(avg_x, avg_y, avg_a));

        out_smoothed_trajectory << (i+1) << " " << avg_x << " " << avg_y << " " << avg_a << endl;
    }
    auto end_smoothing = chrono::high_resolution_clock::now();
    smoothing_time += chrono::duration<double>(end_smoothing - start_smoothing).count();
    // Step 4 - Generate new set of previous to current transform, such that the trajectory ends up being the same as the smoothed trajectory
    vector <TransformParam> new_prev_to_cur_transform;

    // Accumulated frame to frame transform
    auto start_generating = chrono::high_resolution_clock::now();
    a = 0;
    x = 0;
    y = 0;

    for(size_t i=0; i < prev_to_cur_transform.size(); i++) {
        x += prev_to_cur_transform[i].dx;
        y += prev_to_cur_transform[i].dy;
        a += prev_to_cur_transform[i].da;

        // target - current
        double diff_x = smoothed_trajectory[i].x - x;
        double diff_y = smoothed_trajectory[i].y - y;
        double diff_a = smoothed_trajectory[i].a - a;

        double dx = prev_to_cur_transform[i].dx + diff_x;
        double dy = prev_to_cur_transform[i].dy + diff_y;
        double da = prev_to_cur_transform[i].da + diff_a;

        new_prev_to_cur_transform.push_back(TransformParam(dx, dy, da));

        out_new_transform << (i+1) << " " << dx << " " << dy << " " << da << endl;
    }
    auto end_generating = chrono::high_resolution_clock::now();
    generating_time += chrono::duration<double>(end_generating - start_generating).count();
    // Step 5 - Apply the new transformation to the video
    auto start_transform = chrono::high_resolution_clock::now();
    cap.set(cv::CAP_PROP_POS_FRAMES, 0);
    Mat T(2,3,CV_64F);

    VideoWriter outputVideo;
    int frame_width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    cv::Size S = cv::Size(frame_width, frame_height);

    outputVideo.open("output.avi", VideoWriter::fourcc('M','J','P','G'), cap.get(cv::CAP_PROP_FPS), S);
    assert(outputVideo.isOpened());

    int BORDER_CROP = frame_height * 0.25;
    int vert_border = BORDER_CROP * prev.rows / prev.cols; // get the aspect ratio correct

    k=0;
    cap >> cur;
    while(k < max_frames-1) { // don't process the very last frame, no valid transform
        cap >> cur;

        if(cur.data == NULL) {
            break;
        }

        T.at<double>(0,0) = cos(new_prev_to_cur_transform[k].da);
        T.at<double>(0,1) = -sin(new_prev_to_cur_transform[k].da);
        T.at<double>(1,0) = sin(new_prev_to_cur_transform[k].da);
        T.at<double>(1,1) = cos(new_prev_to_cur_transform[k].da);

        T.at<double>(0,2) = new_prev_to_cur_transform[k].dx;
        T.at<double>(1,2) = new_prev_to_cur_transform[k].dy;

        Mat cur2;

        warpAffine(cur, cur2, T, cur.size());

        cur2 = cur2(Range(vert_border, cur2.rows-vert_border), Range(HORIZONTAL_BORDER_CROP, cur2.cols-HORIZONTAL_BORDER_CROP));

        // Resize cur2 back to cur size, for better side by side comparison
        resize(cur2, cur2, S);

        char str[256];
        sprintf(str, "images/%08d.jpg", k);
        imwrite(str, cur2);

        // Now draw the original and stablised side by side for coolness
        Mat canvas = Mat::zeros(cur.rows, cur.cols*2+10, cur.type());
        

        cur.copyTo(canvas(Range::all(), Range(0, cur2.cols)));
        cur2.copyTo(canvas(Range::all(), Range(cur2.cols+10, cur2.cols*2+10)));

        outputVideo << cur2;
        // If too big to fit on the screen, then scale it down by 2, hopefully it'll fit :)
        if(canvas.cols > 1920) {
            resize(canvas, canvas, Size(canvas.cols/2, canvas.rows/2));
        }

        imshow("before and after", canvas); //Para mostrar el video descomentar esta linea

        //char str[256];
        //sprintf(str, "images/%08d.jpg", k);
        imwrite(str, canvas);

        waitKey(20);

        k++;
    }
    auto end_transform = chrono::high_resolution_clock::now();
    transform_time += chrono::duration<double>(end_transform - start_transform).count();


    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> iteration_time = end - start;
    double total_time = iteration_time.count();
     // Resultados finales
    cout << "Optical Flow Time: " << optical_flow_time << " seconds" << endl;
    cout << "Trajectory Calculation Time: " << trajectory_time << " seconds" << endl;
    cout << "Smoothing Time: " << smoothing_time << " seconds" << endl;
    cout << "Generating Time: " << generating_time << " seconds" << endl;
    cout << "Transform Calculation Time: " << transform_time << " seconds" << endl;
    cout << "Tiempo de ejecución: " << total_time << endl;
    outputVideo.release();
    return 0;
}
