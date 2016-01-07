# ATRIUM
### Kinect Recording for Filmmakers

This software works with the Kinect v2 and is Windows Store Compatible. If you have the [Kinect for Windows SDK](https://www.kinectforwindows.com) and [Microsoft Visual Studio Community](https://www.visualstudio.com/en-us/products/visual-studio-community-vs.aspx) (both free) then you should be able to download this repo, hit compile, and go to to the races.

This is the program that was used to record all the depth capture for ["Another Love," a music video for Nyles Lannon](https://vimeo.com/145706460).

On shoot days, subjects could be captured for minute-long takes spread out over the course of several hours. We ultimately rolled the cameras well over 100 times on three separate Kinects, with few losses of data, each of which was tracked in the moment.

## FEATURES

- RECORD depth frames at 30 FPS to a simple, fast binary format
- READ the binary depth stream via PYTHON included in this project – much much faster than OBJ, or
- EXPORT to an OBJ sequence for use in any graphics program, with UVs
- REPLAY takes to see how they went, and view them from different angles
- Dynamically editable shaders – edit your shader code live and watch the image update
- Custom DirectX implementation – get in there and write your own live rendering
- Comes with a Houdini file for easy import and cleanup

## FUTURE FEATURES

- Color capture is coded and tested, but not activated for performance reasons
- The UI flow is weird (although it works for real shoots)
- Joint capture is partly coded
- Camera control is weak
- Playback could use a nice timeline, play/pause buttons, the works.


## JANKY BITS

Do you know Windows Store C++? Do you want to learn? We can use help with the following issues:

- We're currently transforming to 3D coordinates at record-time, which is highly suboptimal
- The file naming code is dumb
- There is a very important while loop that is not running on a properly timed thread (works fine, but is jank)
- UI cannot currently interface effectively with registered event handlers on the Kinect – this is an architecture issue
