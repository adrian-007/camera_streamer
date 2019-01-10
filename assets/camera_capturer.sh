#!/bin/sh

# This script is responsible for capturing video from camera using raspivid (because it allows for
# much finer tuning of camera parameters) and encapsulation in MPEG-TS container via FFmpeg that
# will segment files in HLS-compatible way. Produced segment files are ready for uploading to
# remote server where they can be stored or feed live to clients.

[ "$CAMERA_BASE_URL" != "" ] || exit 1
[ "$CAMERA_BASE_DIR" != "" ] || exit 1

[ "$CAMERA_WIDTH" -eq "$CAMERA_WIDTH" ] 2>/dev/null && [ $CAMERA_WIDTH -gt 0 ] && [ $CAMERA_WIDTH -le 1920 ] || exit 1
[ "$CAMERA_HEIGHT" -eq "$CAMERA_HEIGHT" ] 2>/dev/null && [ $CAMERA_HEIGHT -gt 0 ] && [ $CAMERA_HEIGHT -le 1080 ] || exit 1

[ "$CAMERA_FPS" -eq "$CAMERA_FPS" ] 2>/dev/null && [ $CAMERA_FPS -gt 0 ] && [ $CAMERA_FPS -le 30 ] || exit 1
[ "$CAMERA_SEGMENT_DURATION" -eq "$CAMERA_SEGMENT_DURATION" ] 2>/dev/null && [ $CAMERA_SEGMENT_DURATION -gt 0 ] || exit 1

trap 'trap "" INT TERM; kill -TERM $PID; wait $PID;' INT TERM

raspivid \
    -t 0 \
    -n \
    -fps $CAMERA_FPS \
    -w $CAMERA_WIDTH \
    -h $CAMERA_HEIGHT \
    -ih \
    -fl \
    -stm \
    -pf high \
    -lev 4.1 \
    -qp 20 \
    -b 0 \
    -g $(($CAMERA_FPS*$CAMERA_SEGMENT_DURATION)) \
    -cfx 128:128 \
    -a 1032 \
    -a "%Y-%m-%d %X" \
    -ae 16,0xff,0x808033,2,20,0 \
    -o - | \
    ffmpeg \
        -loglevel error \
        -hide_banner \
        -f h264 \
        -probesize 2048 \
        -analyzeduration 1 \
        -r $CAMERA_FPS \
        -re \
        -i - \
        -c:v copy \
        -f hls \
        -hls_list_size 0 \
        -hls_time 1 \
        -strftime \
        -strftime_mkdir 1 \
        -hls_flags temp_file+independent_segments \
        -hls_segment_filename "$CAMERA_BASE_DIR/%Y-%m-%d/video_%H%M%S.ts" \
        /dev/null &

# This trickery is needed for properly killing child processes (ffmpeg & raspivid) so that they
# can be restarted quickly. In this instance, we'll kill ffmpeg process, which will trigger end
# of raspivid process.
# Idea from http://veithen.io/2014/11/16/sigterm-propagation.html

PID=$!
wait $PID

exit 0;
