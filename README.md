xdeltalib
=========

Xdeltalib 是用 C++ 实现的差异数据提取库，其核心就是 Rsync 的算法。用于提取文件间差异数据，并且用于在两个端点之间进行差异化的文件同步，有一个压缩工具称为 xdelta，但是其所使用的算法原理跟本库完全不同。Xdeltalib 库的特点有如下几个：

    * 完全用 C++ 写成，可以集中到 C++ 项目中，并充分利用 C++ 的优势。　　
    * 支持多平台，在 Windows 与 Linux 中经过严格测试，也可以整合到 Unix 平台中。
    * 代码经过特别优化，差异算法及数据结构经过精心设计，增加了执行性能。
    * 支持 in-place 同步算法，可以应用到各种平台中，包括移动平台、服务器环境以及 PC 环境。
    * 支持可配置的 multi-round （多轮）同步算法，提高同步效率，同时提高了集成平台的可配置性。
    * 集成网络数据传输功能，减少了用户整合的工作量，加快整合进度。
    * 支持可配置的或者默认的线程数，充分利用多核优势，提高了执行性能。
    * 采用消费者与生产者模型提交与处理任务，充分利用并发优势。
    * 一库多用途，即可用于传统的文件数据同步，也可用于其他差异算法可应用的场景。
    * 良好的平台适应性。通过特别的设计，提供在各种存储平台的应用，如单设备环境，云存储环境，以及分存式存储环境。
    * 完备的文档、支持与快速响应。

xdeltalib 可应用的范围可包括：

    * 传统的差异文件同步。
    * 用于云存储平台与移动终端、普通 PC 终端之间的文件同步，可节省大量流量。
    * 可以集成于存储平台之间，用于存储设备之间的数据同步。
    * 可以用于在特定存储设备的重复数据删除。
    * 等等。

xdeltalib 的目标
================
xdeltalib 的目标是提供一个灵活、轻量的 C++ rsync 同步算法库。虽然很多的模块可以为 xdeltalib 提供成熟，稳定的实现，如网络通信的 ACE，boost 库以及各种多线程库。但是为了保证项目的轻量，以及集成的方便，我们宁愿放弃这些库，因为他们太重了。同时也为了能够方便在各种平台中运行，过多的库依赖，会使得移植工作非常困难。

xdeltalib 的代码协议
====================
我们采用 GPL v2.0 来发布我们的代码。你应该手头有一份这样的协议，如果没有，你可以写信到这里获取：Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA。
如果你需要在你的项目中集成本库，请确认你的项目符合 GPL 协议。如果你需要在商业产品中集成，请写信到：yeyouqun@163.com。

关于 xdeltalib logo
====================
文件 xdelta.png 是 xdeltalib 的专用图标，任何人不得在未经许可的情况下，使用在商业产品中，但是可以用于符合 GPL 协议的源代码或者产品，但是必须说明图标的来源。图标的所有人为项目的作者，即 yeyouqun@163.com。

xdeltalib 概念的澄清
====================
在本库中，所使用到的同步模式为：

    1. 如果要将某个源文件的内容同步到目的地，即在目的地上创建一个等待进程;
    2. 源文件端将所要处理的任务（文件）依次提交到本地的处理线程中，多个线程会依次从提交的任务提取并处理完成。
    3. 如果处理完成，则源文件端退出，此时，目的地端的进程也会退出。
    
因此，我们将这个模式中源文件端称为客户端，而目标等称为服务器端。

关于本库的参考论文：
====================
项目参与者可以参考如下论文：

    1. Efficient Algorithms for Sorting and Synchronization，Andrew Tridgell， A thesis submitted for the degree of Doctor of Philosophy at The Australian National University，February 1999。
    2. In-Place Rsync: File Synchronization for Mobile and Wireless Devices，David Rasch and Randal Burns Department of Computer Science Johns Hopkins University {frasch,randalg}@cs.jhu.edu。
    3. J. Langford. Multiround Rsync. Technical Report Available at www.cs.cmu.edu/jcl/research/mrsync/-mrsync.ps, Dept. of Computer Science, Carnegie-Mellon University, 2001.