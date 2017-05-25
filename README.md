[![Stories in Ready](https://badge.waffle.io/01org/intel-vaapi-driver.png?label=ready&title=Ready)](http://waffle.io/01org/intel-vaapi-driver)
[![Build Status](https://travis-ci.org/01org/intel-vaapi-driver.svg?branch=master)](https://travis-ci.org/01org/intel-vaapi-driver)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/11612/badge.svg)](https://scan.coverity.com/projects/01org-intel-vaapi-driver)

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

If you would like to contribute to intel-vaapi-driver, check our [Contributing
guide](https://github.com/01org/intel-vaapi-driver/blob/master/CONTRIBUTING.md).

We also recommend taking a look at the ['janitorial'
bugs](https://github.com/01org/intel-vaapi-driver/issues?q=is%3Aopen+is%3Aissue+label%3AJanitorial)
in our list of open issues as these bugs can be solved without an
extensive knowledge of intel-vaapi-driver.

We would love to help you start contributing!

The intel vaapi media development team can be reached via our [mailing
list](https://lists.01.org/mailman/listinfo/intel-vaapi-media) and on IRC
in channel ##intel-media on [Freenode](https://freenode.net/kb/answer/chat).

We also use [#Slack](https://slack.com) and host [VAAPI Media Slack
Team](https://intel-media.slack.com).  You can signup by submitting your email
address to our [Slack Team invite page](https://slack-join-intel-media.herokuapp.com).

Slack complements our other means of communication.  Pick the one that works
best for you!
