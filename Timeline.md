### 现阶段项目进程：
完成原型机的制作，仍含大量Bug
最大的bug已修复20201113
### 本周末安排如下：20200926
@吴** 我主要负责程序部分的完善，把bug备忘录里面那个大问题给解决了，以及输出部分的函数确实也该整理优化精简一下了。<br/>
@向** 寻找新的物联网平台，包括但不限于阿里系的阿里云以天猫精灵为核心的物联网平台、腾讯系的以QQ为核心的物联网平台，小米以小爱同学为核心的物联网开放平台、机智云（号称可零代码写APP）、AbleCloud等，评估各个平台的利弊，重点考虑对象是技术实现难度，优先使用热门的平台，还需考虑各种因素，找到最适合我们使用的。需要对各个平台进行注册以及使用体验，不能仅看介绍，最好能写一个简短的文档出来记录选择的过程以及结果，这样我们好上交进度报告。<br/>
（我们的需求是能够制作出一个小程序或APP，能推送消息提醒，能显示统计数据，能上位机上对全局控制变量进行控制，平台需要满足这些要求。）<br/>
@叶** 对产品的外包装的设计以及实现方法进行的一个初步的想象，设计美观和实现难度都要考虑到。<br/>
@成** 跟我一起解决现有的程序问题，修bug，负责调试程序。<br/>
### 完成原型机不代表项目快要做完了，而是才刚刚开始！！！
现在代码存在很多的问题，除了修复bug外，还需要改写库文件，如果不深入库文件的话，我们做的事情和小学生无异！<br/>
经测试，贝壳物联的效果并不好，需要使用新的平台，其次，目前仅仅实现的是数据的传出，还没有实现在上位机上对全局控制变量进行控制。<br/>
机械结构的设计难度并不简单，使用杜邦线连接容易松动，且线路复杂，占体积大，在设计机械部分的时候要充分考虑到机械部分的设计，不可能使用一个整体吧，需要使用多部件拼接吧，所以无论是3D打印还是其它方式制作，以及需要考虑到美观性，这些事情做好均需要一定的时间。<br/>
论文方面，选材比较麻烦，看来也只能写整体上的，我看看能不能水一篇。<br/>
关于演示视频，拍摄以及剪辑出好的效果也并不简单，需要一定的时间，目前还不确定结题是否需要演示视频，但是这个项目做好以后我们可以去参加其它的各种比赛，可能就需要演示视频，早做未免不可<br/>
### 下一阶段进度安排：
以下内容未确定,处于草稿状态，暂时不这么安排<br/>
由于软件部分基本实现，现对整体项目分工进行重新安排：<br/>
向**  负责   物联网部分的继续实现，换平台，以及一个简单的微信小程序的制作。<br/>
叶**  负责   机械部分的设计以及走线，即外壳的设计，包括但不局限于使用3D打印，注意美观。<br/>
成**    负责   <br/>
吴**  负责   项目总体规划，程序代码整体优化，论文撰写，期刊的寻找与投稿 <br/>
<br/>

2020年11月前，完成外壳的最终设计,即10月中旬就需要完成初步方案。<br/>
2021年2月前，论文见刊，完成结题所需的一切的条件。<br/>
2021年5月，可使用此项目参加ICAN大赛（B类赛事），我觉得这个赛事是有点水的。<br/>
具体的详细时间安排待补充。<br/>
### 版本控制注意：
由于github版本控制效果很好，我们无需进行严格的版本号规则，所以中期检查结束以前我们统一使用外部版本号为v1.0，不修改版本号
### 更新代码注意：
先创建一个新的分支，然后此分支就会为最新的master版本的代码，在此分支上修改，然后确定无致命bug后,pull request 请求merge,即可提交合并到master。小修改可以不写修改说明。<br/>
也可以不创建新的分支，在pull request里面可以merge from master into your branch，这样就可以更新你的分支里面的代码进度与master一致，注意最后你修改完了后，执行的是是反操作也即merge form your branch into master.
### arduino IDE中上传项目出现timeout注意：
先断开ESP8266的电源再上传即可。