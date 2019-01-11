# Camera Streamer
Application that streams video segment files to predefined remote location.

Project contains two main components: streamer application and capturer script.

1. Streamer is responsible for sending files to remote location and controlling
   congestion and segments backlog.
2. Capturer is responsible for grabbing H264 video via raspivid application
   and at the same time segmenting it using FFmpeg's hls muxer. Segment
   files are saved in a predefined location for pickup by the streamer.


Capturer is written specifically for Raspberry Pi (relies on raspivid), but segment
files can be delivered by other means as long as valid configuration is provided
for the streamer component (which is device-agnostic).


## Congestion control and backlog management
Usually segment files has fixed duration, so if application is configured to capture
10 seconds of video stream and put it to segment file, we have 10 seconds for sending
that file. To control congestion while sending segments, streamer queues files from newest
to oldest and creates a time window that it has for sending files until new segment arrives.
Time window for sending batch of segments is 90% of single segment duration - given default
10 seconds for each segment, that's 9 seconds. After sending a segment, time spent on sending
it is subtracted from time window. Segments are picked up from queue until there's time left
in time window.

This behaviour is different that usual streaming via UPD sockets in a way that segments
backlog is more resilient to short network disruptions. In that case stream won't be
missing frames - unless streaming device doesn't have needed bandwidth capacity to
clear the backlog.

Due to storage constraints backlog have a defined duration that, when exceeded, will
force streamer to drop oldest segments from queue, permanently removing them from
storage. This precaution must be taken in order to avoid running out of storage
space that would disallow saving new segments.


## Remote location
Any decent HTTP server that supports PUT and DELETE method will be able to receive
segment files. Using HTTP server allows access control (authentication) and transport security
(HTTPS) out of the box. Due to privacy reasons using HTTPS is highly encouraged.

Example configuration for nginx:

```
http {
    # ...
    server {
        # ...

        location /camera {
            # Uncomment it if you want to have this location indexed by nginx.
            #autoindex on;

            # Uncomment it if you want to restrict access to this location.
            # Remember to set proper path and appropriate access rights to password file.
            #auth_basic "Restricted";
            #auth_basic_user_file /etc/nginx/.htpasswd;

            client_body_temp_path /var/www/camera/.client_tmp;
            client_max_body_size 10m;

            dav_methods PUT DELETE MKCOL COPY MOVE;
            dav_access group:rw  all:r;
            create_full_put_path on;
        }
    }
}
```


## Configuration
Configuration is done by environment variables - they must be set before
launching either capturer or streamer components.

List of possible values:

1. ``CAMERA_BASE_URL`` - Base URL where segment files will be transferred to.
2. ``CAMERA_USERNAME`` - If set, then username - along with password - will be
   included in HTTP requests to server (currently only Basic authentication
   method is supported).
3. ``CAMERA_PASSWORD`` - Must be set if username is.
4. ``CAMERA_BASE_DIR`` - Base directory where capturer will save and streamer will
   look for video segments file.
5. ``CAMERA_WIDTH`` - video width in pixels. Defaults to 1280.
6. ``CAMERA_HEIGHT`` - video height in pixels. Defaults to 720.
7. ``CAMERA_FPS`` - video frame rate. Defaults to 5.
8. ``CAMERA_SEGMENT_DURATION`` - individual segment duration in seconds. Defaults to 10.
9. ``CAMERA_MAX_BACKLOG_TOTAL_SEGMENT_DURATION`` - maximal total duration of segments in
   the backlog. Defaults to 43200 (12 hours, approx. 1.5 GB of video files). Set to 0 keep
   all files in the backlog (dangerous, can run out of storage).
10. ``CAMERA_SEGMENT_EXTENSIONS`` - list of segment file extensions to look for in
    base directory. Defaults to '.ts .mp4'
