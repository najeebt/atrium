# ATRIUM
### Kinect Recording Software for Filmmakers
by Najeeb Tarazi

Atrium is the Kinect for Windows Interface that was used to record all the depth capture for ["Another Love," a music video for Nyles Lannon](https://vimeo.com/145706460). On shoot days, subjects could be captured for minute-long takes, and we did dozens of takes spread out over the course of several hours.

This software works with the Kinect v2 and is Windows Store Compatible. If you have the [Kinect for Windows SDK](https://www.kinectforwindows.com) and [Microsoft Visual Studio Community](https://www.visualstudio.com/en-us/products/visual-studio-community-vs.aspx) (both free) then you should be able to download this repo, hit compile, and go to to the races.

The software is currently in the patchy state that it was in for the shoot -- very functional for recording depth frames at 30fps and feeding those frames to Houdini. Some other features are in the code, but dormant. Contact me if you have questions, and read on to see what's in the code, what's planned for the future, and how you can help.

## FEATURES

- RECORD depth frames at 30 FPS to a simple, fast binary format
- READ the binary depth stream via PYTHON available on request (in project soon) – much much faster than OBJ, or
- EXPORT to an OBJ sequence for use in any graphics program
- REPLAY takes to see how they went, and view them from different angles
- Dynamically editable shaders – edit your shader code live and watch the image update
- Custom DirectX implementation – get in there and write your own live rendering

## FUTURE FEATURES

- Color capture is coded and tested, but not activated for performance reasons
- Write to Alembic and/or Pixar's USD format
- UI improvements (although the current setup works for real shoots)
- Joint capture is partly coded
- Camera control is weak
- Playback could use a nice timeline, play/pause buttons, the works.


## JANKY BITS

Do you know Windows Store C++? Do you want to learn? Here are some places WE COULD USE YOUR HELP!

- We're currently transforming to 3D coordinates at record-time, which is highly suboptimal. Ideally you just record the depth frames and transform them into 3D coordinates when you're ready.
- There's a system 2G file size limit which requires pre-generating a set of files to which we can stream data. The magic number of these files right now is 10. I would love to find a way to write to a single file (the key, again, may be in an Alembic or USD library)
- The file naming code is dumb
- There is a very important while loop that is not running on a properly timed thread (works fine, but is jank)
- UI cannot currently interface effectively with registered event handlers on the Kinect to active/deactivate different types of feeds – this is an architecture issue
- Cleanup and refactoring is always welcome (as long as the program still works!)

Please contact me if you'd like to contribute or you have any questions!

### License

Licensed under the Apache License, Version 2.0. Please see the LICENSE and NOTICE files in the root directory of this project for license information.
