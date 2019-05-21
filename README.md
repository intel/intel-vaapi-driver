[![Stories in Ready](https://badge.waffle.io/intel/intel-vaapi-driver.png?label=ready&title=Ready)](http://waffle.io/intel/intel-vaapi-driver)
[![Build Status](https://travis-ci.org/intel/intel-vaapi-driver.svg?branch=master)](https://travis-ci.org/intel/intel-vaapi-driver)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/11612/badge.svg)](https://scan.coverity.com/projects/intel-intel-vaapi-driver)

# Intel-vaapi-driver Project

VA-API (Video Acceleration API) user mode driver for Intel GEN Graphics family

VA-API is an open-source library and API specification, which
provides access to graphics hardware acceleration capabilities
for video processing. It consists of a main library and
driver-specific acceleration backends for each supported hardware 
vendor.

The current video driver backend provides a bridge to the GEN GPUs through the packaging of buffers and
commands to be sent to the i915 driver for exercising both hardware and shader functionality for video
decode, encode, and processing.

## Platform definitions

* CTG: Cantiga, Intel GMA 4500MHD (GM45)
* ILK: Ironlake, Intel HD Graphics for 2010 Intel Core processor family
* SNB: Sandybridge, Intel HD Graphics for 2011 Intel Core processor family
* IVB: Ivybridge
* HSW: Haswell
* BDW: Broadwell
* CHV/BSW: Cherryview/Braswell
* SKL: Skylake
* BXT: Broxton
* KBL: Kabylake
* GLK: Gemini Lake
* CFL: Coffee Lake
* CNL: Cannolake
* ICL: Ice Lake

## Supported platforms

| Codec          | Decode    | Encode    |
|----------------|-----------|-----------|
| H.264          | ILK+      | SNB+      |
| MPEG-2         | CTG+      | -         |
| VC-1           | SNB+      | -         |
| JPEG           | IVB+      | CHV+/BSW+ |
| VP8            | BDW+      | CHV+/BSW+ |
| HEVC           | CHV+/BSW+ | SKL+      |
| HEVC 10-bit    | BXT+      | KBL+      |
| VP9            | BXT+      | KBL+      |
| VP9 10-bit     | KBL+      | ICL+      |
| HEVC/VP9 4:4:4 | ICL+      | ICL+      |

## Requirements

libva >= 2.1.0

## Testing

Please read the [TESTING](https://github.com/intel/intel-vaapi-driver/blob/master/TESTING) file available in this package.

## Contibuting

If you would like to contribute to intel-vaapi-driver, check our [Contributing
guide](https://github.com/intel/intel-vaapi-driver/blob/master/CONTRIBUTING.md).

We also recommend taking a look at the ['janitorial'
bugs](https://github.com/intel/intel-vaapi-driver/issues?q=is%3Aopen+is%3Aissue+label%3AJanitorial)
in our list of open issues as these bugs can be solved without an
extensive knowledge of intel-vaapi-driver.

We would love to help you start contributing!

## Communication

The intel vaapi media development team can be reached via our [mailing
list](https://lists.01.org/mailman/listinfo/intel-vaapi-media) and on IRC
in channel ##intel-media on [Freenode](https://freenode.net/kb/answer/chat).

We also use [#Slack](https://slack.com) and host [VAAPI Media Slack
Team](https://intel-media.slack.com).  You can signup by submitting your email
address to our [Slack Team invite page](https://slack-join-intel-media.herokuapp.com).

Slack complements our other means of communication.  Pick the one that works
best for you!

## License

Please read the [COPYING](https://github.com/intel/intel-vaapi-driver/blob/master/COPYING) file available in this package.
