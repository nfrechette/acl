# Research: Arithmetic Accuracy

In order to investigate the accuracy and performance implications of using **float32**, **float64**, or **fixed point** arithmetic, a research branch was created.

Supporting **float32** and **float64** in parallel and comparing them extensively proved somewhat simple with the help of templates. However, **fixed point** arithmetic differs considerably and instead a playground was created to investigate.

The conclusions can be found in detail [here](https://nfrechette.github.io/2017/12/29/acl_research_arithmetic/) and the code lives [here](https://github.com/nfrechette/acl/tree/research/float-vs-double-vs-fixed-point).
