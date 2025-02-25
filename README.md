# Video Stabilization Project

## Description
This project implements video stabilization using optical flow algorithms. It processes video files to smooth out the trajectory of the camera movement, resulting in a stabilized output video.

## Installation
To run this project, you need to install OpenCV 4.x from their GitHub repository(with the contrib module).

## Usage
To run the stabilization program, use the following command:
```bash
./stabilizer [video.avi] [optical_flow_algorithm]
```
Where `[video.avi]` is the path to the input video file and `[optical_flow_algorithm]` can be:
- 1: LK Sparse
- 2: LK Dense
- 3: Farneback

Example:
```bash
./stabilizer input_video.avi 2
```

## Output
The program generates the following output files:
- `output.avi`: The stabilized video.
- `prev_to_cur_transformation.txt`: Transformation data for each frame.
- `trajectory.txt`: The trajectory of the camera movement.
- `smoothed_trajectory.txt`: The smoothed trajectory data.
- `new_prev_to_cur_transformation.txt`: New transformation data based on the smoothed trajectory.

## License
This project is licensed under the MIT License. See the LICENSE file for more details.
