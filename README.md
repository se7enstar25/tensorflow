<div align="center">
  <img src="https://www.tensorflow.org/images/tf_logo_transp.png"><br><br>
</div>
-----------------

| **`Linux CPU`** | **`Linux GPU`** | **`Mac OS CPU`** | **`Windows CPU`** | **`Android`** |
|-----------------|---------------------|------------------|-------------------|---------------|
| [![Build Status](https://ci.tensorflow.org/buildStatus/icon?job=tensorflow-master-cpu)](https://ci.tensorflow.org/job/tensorflow-master-cpu) | [![Build Status](https://ci.tensorflow.org/buildStatus/icon?job=tensorflow-master-linux-gpu)](https://ci.tensorflow.org/job/tensorflow-master-linux-gpu) | [![Build Status](https://ci.tensorflow.org/buildStatus/icon?job=tensorflow-master-mac)](https://ci.tensorflow.org/job/tensorflow-master-mac) | [![Build Status](https://ci.tensorflow.org/buildStatus/icon?job=tensorflow-master-win-cmake-py)](https://ci.tensorflow.org/job/tensorflow-master-win-cmake-py) | [![Build Status](https://ci.tensorflow.org/buildStatus/icon?job=tensorflow-master-android)](https://ci.tensorflow.org/job/tensorflow-master-android) |

**TensorFlow** is an open source software library for numerical computation using
data flow graphs.  Nodes in the graph represent mathematical operations, while
the graph edges represent the multidimensional data arrays (tensors) that flow
between them.  This flexible architecture lets you deploy computation to one
or more CPUs or GPUs in a desktop, server, or mobile device without rewriting
code.  TensorFlow also includes TensorBoard, a data visualization toolkit.

TensorFlow was originally developed by researchers and engineers
working on the Google Brain team within Google's Machine Intelligence research
organization for the purposes of conducting machine learning and deep neural
networks research.  The system is general enough to be applicable in a wide
variety of other domains, as well.

**If you'd like to contribute to TensorFlow, be sure to review the [contribution
guidelines](CONTRIBUTING.md).**

**We use [GitHub issues](https://github.com/tensorflow/tensorflow/issues) for
tracking requests and bugs, but please see
[Community](tensorflow/docs_src/about/index.md#community) for general questions
and discussion.**

## Installation
*See [Installing TensorFlow](https://www.tensorflow.org/install/) for instructions on how to install our release binaries or how to build from source.*

People who are a little more adventurous can also try our nightly binaries:

* Linux CPU-only: [Python 2](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-cpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON2,label=cpu-slave/lastSuccessfulBuild/artifact/pip_test/whl/tensorflow-1.1.0rc1-cp27-none-linux_x86_64.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-cpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON2,label=cpu-slave)) / [Python 3.4](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-cpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON3,label=cpu-slave/lastSuccessfulBuild/artifact/pip_test/whl/tensorflow-1.1.0rc1-cp34-cp34m-linux_x86_64.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-cpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON3,label=cpu-slave/)) / [Python 3.5](https://ci.tensorflow.org/view/Nightly/job/nightly-python35-linux-cpu/lastSuccessfulBuild/artifact/pip_test/whl/tensorflow-1.1.0rc1-cp35-cp35m-linux_x86_64.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-python35-linux-cpu/))
* Linux GPU: [Python 2](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-linux-gpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON2,label=gpu-linux/lastSuccessfulBuild/artifact/pip_test/whl/tensorflow_gpu-1.1.0rc1-cp27-none-linux_x86_64.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-linux-gpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON2,label=gpu-linux/)) / [Python 3.4](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-linux-gpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON3,label=gpu-linux/lastSuccessfulBuild/artifact/pip_test/whl/tensorflow_gpu-1.1.0rc1-cp34-cp34m-linux_x86_64.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-linux-gpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON3,label=gpu-linux/)) / [Python 3.5](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-linux-gpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON3.5,label=gpu-linux/lastSuccessfulBuild/artifact/pip_test/whl/tensorflow_gpu-1.1.0rc1-cp35-cp35m-linux_x86_64.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-linux-gpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON3.5,label=gpu-linux/))
* Mac CPU-only: [Python 2](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-cpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON2,label=mac-slave/lastSuccessfulBuild/artifact/pip_test/whl/tensorflow-1.1.0rc1-py2-none-any.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-cpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON2,label=mac-slave/)) / [Python 3](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-cpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON3,label=mac-slave/lastSuccessfulBuild/artifact/pip_test/whl/tensorflow-1.1.0rc1-py3-none-any.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-cpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON3,label=mac-slave/))
* Mac GPU: [Python 2](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-mac-gpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON2,label=gpu-mac/lastSuccessfulBuild/artifact/pip_test/whl/tensorflow_gpu-1.1.0rc1-py2-none-any.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-mac-gpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON2,label=gpu-mac/)) / [Python 3](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-mac-gpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON3,label=gpu-mac/lastSuccessfulBuild/artifact/pip_test/whl/tensorflow_gpu-1.1.0rc1-py3-none-any.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-matrix-mac-gpu/TF_BUILD_IS_OPT=OPT,TF_BUILD_IS_PIP=PIP,TF_BUILD_PYTHON_VERSION=PYTHON3,label=gpu-mac/))
* Windows CPU-only: [Python 3.5 64-bit](https://ci.tensorflow.org/view/Nightly/job/nightly-win/DEVICE=cpu,OS=windows/lastSuccessfulBuild/artifact/cmake_build/tf_python/dist/tensorflow-1.1.0rc1-cp35-cp35m-win_amd64.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-win/DEVICE=cpu,OS=windows/))
* Windows GPU: [Python 3.5 64-bit](https://ci.tensorflow.org/view/Nightly/job/nightly-win/DEVICE=gpu,OS=windows/lastSuccessfulBuild/artifact/cmake_build/tf_python/dist/tensorflow_gpu-1.1.0rc1-cp35-cp35m-win_amd64.whl) ([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-win/DEVICE=gpu,OS=windows/))
* Android: [demo APK](https://ci.tensorflow.org/view/Nightly/job/nightly-android/lastSuccessfulBuild/artifact/out/tensorflow_demo.apk), [native libs](http://ci.tensorflow.org/view/Nightly/job/nightly-android/lastSuccessfulBuild/artifact/out/native/)
([build history](https://ci.tensorflow.org/view/Nightly/job/nightly-android/))

#### *Try your first TensorFlow program*
```shell
$ python
```
```python
>>> import tensorflow as tf
>>> hello = tf.constant('Hello, TensorFlow!')
>>> sess = tf.Session()
>>> sess.run(hello)
Hello, TensorFlow!
>>> a = tf.constant(10)
>>> b = tf.constant(32)
>>> sess.run(a+b)
42
>>>
```

##For more information

* [TensorFlow website](http://tensorflow.org)
* [TensorFlow whitepaper](http://download.tensorflow.org/paper/whitepaper2015.pdf)
* [TensorFlow Model Zoo](https://github.com/tensorflow/models)
* [TensorFlow MOOC on Udacity](https://www.udacity.com/course/deep-learning--ud730)

The TensorFlow community has created amazing things with TensorFlow, please see the [resources section of tensorflow.org](https://www.tensorflow.org/versions/master/resources#community) for an incomplete list.
