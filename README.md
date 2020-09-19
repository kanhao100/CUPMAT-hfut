# CUPMAT-hfut
## 2020年合肥工业大学校级大创项目—智能物联网水杯垫
### 现阶段项目进程：
完成原型机的制作，仍含大量Bug
## 完成原型机不代表项目快要做完了，而是才刚刚开始！！！
现在代码存在很多的问题，除了修复bug外，还需要改写库文件，如果不深入库文件的话，我们做的事情和小学生无异！<br/>
经测试，贝壳物联的效果并不好，需要使用新的平台，其次，目前仅仅实现的是数据的传出，还没有实现在上位机上对全局控制变量进行控制。
### 下一阶段进度安排：
由于软件部分基本实现，现对整体项目分工进行重新安排：<br/>
向勉益 负责  物联网部分的继续实现，换平台，以及一个简单的微信小程序的制作。<br/>
叶文乐 负责  机械部分的设计以及走线，即外壳的设计，包括但不局限于使用3D打印，<br/>
成帅   负责  <br/>
吴韦举 负责  项目总体规划，。程序代码整体优化，论文撰写，期刊的寻找与投稿，以及 <br/>
2020年10月1日前，完成中期检查的所有准备，中期检查我定的要求是不用设计外壳但需要做好答辩，有演示视频的准备。<br/>
2021年2月前，论文见刊，完成结题所需的一切的条件。<br/>
2021年5月，可使用此项目参加ICAN大赛（B类赛事），我觉得这个赛事是有点水的。<br/>
以上内容未确定,仍可讨论更改。
### 版本控制注意：
由于github版本控制效果很好，我们无需进行严格的版本号规则，所以中期检查结束以前我们统一使用外部版本号为v1.0，不修改版本号
### 更新代码注意：
先创建一个新的分支，然后此分支就会为最新的master版本的代码，在此分支上修改，然后确定无致命bug后,pull request 请求merge,即可提交合并到master。小修改可以不写修改说明。<br/>
也可以不创建新的分支，在pull request里面可以merge from master into your branch，这样就可以更新你的分支里面的代码进度与master一致，注意最后你修改完了后，执行的是是反操作也即merge form your branch into master
### 上传项目出现timeout注意：
先断开ESP8266的电源或者串口通讯再上传即可。
