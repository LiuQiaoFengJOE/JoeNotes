# JoeNotes


**1如何打开笔记**
	下载一个obisidian后直接以项目的类型打开文件夹即可自动加载.

**2移动端没有ssh如何进行远程推送**
	1.在github中生成一个
		setting--->>>Developer settings --->>> personal access tokens --->>> fine-grained tokens 
	2.生成后与httpclone仓库进行拼接得到一个远程地址
		格式https://LiuQiaoFengJOE:XXXXXXXXX@github.com/LiuQiaoFengJOE/JoeNotes.git
	3.想办法把电脑端的整个工程目录拷贝到手机端，后用手机端obisidian 打开工程
	3.在手机端或者平板端打开终端，搜索git edit remote ,替换其中的远程地址即可

![[Pasted image 20260611143817.png]]