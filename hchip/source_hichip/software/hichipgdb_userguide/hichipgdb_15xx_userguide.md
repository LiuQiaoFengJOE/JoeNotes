# Hichip GDB使用手册

```txt
下载地址：https://gitlab.hichiptech.com:62443/sw/tools.git
其中：
\tools\GDB\HiChipGDB-H15\HiChipGDB@H1512 为H15xx系列芯片用的gdb，其中A210/A110/B200/B100等都为H15xx系列。
\tools\GDB\HiChipGDB-H16\HiChipGDB-0.1.0为H16xx系列芯片用的gdb，其中A3100/B3100/C3100/D5000等都为H16xx系列。
\tools\GDB\Driver 为对应gdb的驱动文件。
```

## 1、仿真器配置

### 1.1 WIN10安装驱动

```txt
* 如果电脑上没有安装驱动，在设备管理器中显示的是FX3（如图1.1所示）；
* 鼠标左键点击选中，右键选择更新驱动程序，会弹出对话框，之后选择选择浏览我的电脑以查找驱动程序（如图1.2所示）；
* 在WINDOW上面找到存放HichipGDB驱动的位置（如图1.3所示）；
* 点击确定之后，更新驱动成功（如图1.4所示）；
* 在设备管理器中查看已经安装成功的设备，路径：设备管理器-> 通用串行总线控制器->Cypress FX3 USB BulkloopExample Device（如图1.5所示）。
```

![image-20220411110514177](pic/image-20220411110514177.png)

<center>图1.1 WIN10 GBD未安装驱动</center>

![image-20220411110713138](pic/image-20220411110713138.png)

<center>图1.2 WIN10 浏览我的电脑以查找驱动程序</center>

![image-20220411110938913](pic/image-20220411110938913.png)

<center>图1.3 WIN10 上选择对应的驱动路径</center>

![image-20220411111004753](pic/image-20220411111004753.png)

<center>图1.4 WIN10 已成功更新了驱动</center>

![image-20220218104023172](pic/image-20220218104023172.png)

<center>图1.5 设备管理器中查看已经安装成功的设备</center>

### 1.2 打开HiChipGDB.exe，

![3b259f6609f10d479d77343aa4091802](.\h15xx_pic\3b259f6609f10d479d77343aa4091802.png)

#### 1.2.1 进行平台的设置 ICE -> Platform Setting

![fafa591ddc3cb34c9487187109b105d7](.\h15xx_pic\fafa591ddc3cb34c9487187109b105d7.png)

<center>图1.6 WINGDB 平台设置</center>

### 1.3 Platform Setting参数说明

```txt
选项如图1.7 所示，DDR file（DDR内存初始化abs） ，DDR size（DDR内存），CPU Mode选择 Single Core（单核）
notes：H15xx 系列 RunMode选择EEPROM, CPU Mode选择Single Core。
```

![c9c71d4495e0a144aff8caf4560c7218](.\h15xx_pic\c9c71d4495e0a144aff8caf4560c7218.png)

<center>图1.7 Platform Setting</center>

```txt
H15xx系列可以根据hcrtos\output\images目录下的aux_code_ddrx_xxxM_xxMHZ.abs来选择。例如本次编译出来的是aux_code_ddr2_64M_1066MHz.abs（如图1.8所示），所以选择的文件是\board\hc15xx\common\ddrinit\gdb\gdb_hc15xx_ddr2_64M_1066MHz.abs（如图1.9所示）。
```

![20001be81a0b1440aae8ccf1fa055166](.\h15xx_pic\20001be81a0b1440aae8ccf1fa055166.png)

<center>图1.8 hclinux\buildroot\output\images\目录下的内容</center>

```txt
在\hcrtos\board\hc15xx\common\ddrinit\gdb目录下选择gdb_hc15xx_ddr2_64M_1066MHz.abs，如果没有该目录，请解压gdb.rar
```

![17ba63aa57657b40b5ff56ac973ab236](.\h15xx_pic\8fa640bd784871438f6cdf79bd652c2c.png)

<center>图1.9 hclinux\buildroot\output\images\目录下的内容</center>



## 2、软件烧录软件到flash

### 2.1、配置说明

按照步骤1.1、1.2、1.3 进行配置之后在进行操作。（如果已经配置过了可以忽略）

![](.\h15xx_pic\b9dc48ef0514ba4287d87ed4447ce804.png)

<center>图2.1.1 软件配置成功后的界面</center>

### 2.2 使用HichipGBD烧入程序

#### 2.2.1 选择ICE->init ICE

![2029c289e1924c42a2f92b9f96f4c0e2](.\h15xx_pic\2029c289e1924c42a2f92b9f96f4c0e2.png)

<center>图2.2.1 选择ICE-> init ICE</center>

#### 2.2.2 软件弹出ICE initialize，点击OK

```txt
说明：Plaform选择 H1512，Run Mode选择 EEPROM，初始化成功显示是浅蓝色如图2.2.3所示
```

![9647f06c56d5a045a4dd12164393b34a](.\h15xx_pic\9647f06c56d5a045a4dd12164393b34a.png)

<center>图2.2.2 ICE -> init ICE</center>

#### 2.2.3 初始化成功。初始化不成功请查看2.3 步骤

![212f53eae3968f4fbf69071dc7635f6f](.\h15xx_pic\212f53eae3968f4fbf69071dc7635f6f.png)

<center>图2.2.3 ICE -> init ICE初始化成功显示</center>

#### 2.2.4 初始化成功之后，ICE->DownLoad选择sfburn.ini

![image-20220409151639580](pic/image-20220409151639580.png)

<center>图2.2.4 点击ICE -> Download</center>



#### 2.2.5 选择sfburn.ini 烧入程序

![2cb8daf62c431541a2ab3c56a2341795](.\h15xx_pic\2cb8daf62c431541a2ab3c56a2341795.png)

<center>图2.2.5 选择sfburn.ini</center>

#### 2.2.6 程序预写入过程

![6cd63da7d9e245469bf8d36b7d985070](.\h15xx_pic\6cd63da7d9e245469bf8d36b7d985070.png)

<center>图2.2.6 程序预写入过程</center>



![93a827010be6f24f974fc6b85710cb5e](.\h15xx_pic\93a827010be6f24f974fc6b85710cb5e.png)

<center>图2.2.7 程序预写入成功，可按F5 执行烧入到FLASH</center>



#### 2.2.7 烧入成功

![image-20220411114214037](pic/image-20220411114214037.png)

<center>图2.2.8 烧写成功如图所示</center>

#### 2.2.8 烧写成功复位或者重新上电重启串口打印

![image-20220409152830422](pic/image-20220409152830422.png)

<center>图2.2.9 程序运行</center>

#### 2.2.9 串口输入root 登录

![image-20220409152907279](pic/image-20220409152907279.png)

<center>图2.2.10 程序运行 root登录</center>



### 2.3 初始化不成功，弹出 please reset the CPU(如图2.3.1)

```txt
解决方法：

方法1、使用硬件复位键进行复位。

方法2、可先把仿真器断开，重新上电后再插入仿真器接口。

方法3、删除注册表
 3.1 、首先按下键盘“Win+R”打开运行；
 3.2 、接着输入“regedit” ，再点击“确定”打开注册表；
 3.3、搜索 计算机\HKEY_CURRENT_USER\SOFTWARE；
 3.4 、删除已经注册的内容，如图3.3.2所示，然后重新打开HichipGDB软件。
 
方法4、如果已经关闭了HichipGDB软件，但是在任务管理器却还显示了HichipGDB(32位)的任务，鼠标点击右键，进行结束任务，如图2.3.3所示。

方法5、是否硬件上是否使能了仿真功能，如图2.3.4所示。
```

![image-20220409155133288](pic/image-20220409155133288.png)

<center>图2.3.1 仿真器弹出please reset the risc</center>

![image-20220409163447429](pic/image-20220409163447429.png)

<center>图2.3.2 删除win上注册表的信息</center>



![image-20220411104839557](.\h15xx_pic\53771b6f09cd6d4ba9b33da5ab015563.png)

<center>图2.3.3 WIN10上关闭了HichipGDB软件之后，但还是有HichipGDB MFC Application的任务</center>

![image-20220221101027247](pic/image-20220221101027247.png)

<center>图2.3.4 EJTAG 仿真功能需要使能</center>



