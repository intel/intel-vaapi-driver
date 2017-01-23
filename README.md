#Intel-vaapi-driver Project

VA-API (Video Acceleration API) user mode driver for Intel GEN Graphics family

VA-API is an open-source library and API specification, which
provides access to graphics hardware acceleration capabilities
for video processing. It consists of a main library and
driver-specific acceleration backends for each supported hardware 
vendor.

The current video driver backend provides a bridge to the GEN GPUs through the packaging of buffers and
commands to be sent to the i915 driver for exercising both hardware and shader functionality for video
decode, encode, and processing.

If you would like to contribute to libva, check our [Contributing
guide](https://github.com/01org/intel-vaapi-driver/blob/master/CONTRIBUTING.md).

We also recommend taking a look at the ['janitorial'
bugs](https://github.com/01org/intel-vaapi-driver/issues?q=is%3Aopen+is%3Aissue+label%3AJanitorial)
in our list of open issues as these bugs can be solved without an
extensive knowledge of intel-vaapi-driver.

We would love to help you start contributing!

The libva development team can be reached via our [mailing
list](http://lists.freedesktop.org/mailman/listinfo/libva) and on IRC
in channel #intel-media on [Freenode](https://freenode.net/kb/answer/chat).
