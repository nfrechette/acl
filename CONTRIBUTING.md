# How to contribute

Thank you for your interest in the Animation Compression Library! All contributions that follow our guidelines below and abide by our [code of conduct](CODE_OF_CONDUCT.md) are welcome. Together we can rid the video game world of slippery feet and jittery hands!

In this document you will find relevant reading material, what contributions we are looking for, and what we are *not* looking for.

Project contact email: zeno490@gmail.com

# Getting set up

See the [getting started](./docs/getting_started.md) section for details on how to generate project solutions, build, and run the unit tests.

Every pull request should trigger continuous integration on every platform we support and most of them will also execute the unit tests automatically (except on Android and iOS).

The project roadmap for the next few milestones is tracked with [GitHub issues](https://github.com/nfrechette/acl/issues). [Backlog issues](https://github.com/nfrechette/acl/milestone/4) are things I would like to get done eventually but that have not been prioritized yet.

Whether you create an issue or a pull request, I will do my best to comment or reply within 48 hours.

# Contributions we are looking for

Several issues already have a [help wanted](https://github.com/nfrechette/acl/issues?q=is%3Aopen+is%3Aissue+label%3A%22help+wanted%22) label. Those tasks should be either reasonably simple or tasks that I do not think I will get the chance to get to very soon.

Feature requests are welcome providing that they fit within the project scope. For smaller features, you are welcome to create an issue with as much relevant information as you can. If it makes sense, I will prioritize it or add it to the backlog, and if I feel it is beyond the scope of the project, I will tell you why and close the issue. For larger research topics to investigate, the best way to move forward is to reach out by email. I do want to pursue several larger topics for research but it does not make sense to maintain each one of them within the main branches. Some might ultimately be included but those type of contributions are best done in a parallel research branch.

# Contributions we are *not* looking for

A lot of older compilers do not properly support **C++11** and there is no plan to support them. This also applies to platforms that are either not mainstream or that we cannot easily test with continuous integration. If you would like to support such an exotic environment, reach out by email first so we can discuss this.

This library's focus is solely on animation compression. There is no plan to include blending of animation poses or state machines to drive that process. Other middlewares and libraries already cover that field reasonably well.

If you aren't sure, don't be afraid to reach out by email or on discord!
