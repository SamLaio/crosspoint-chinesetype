# 版本信息
本项目处于初步阶段，而且这是我第一次写固件，欢迎大家指导批评~

基于 **crosspoint 0.16.0** 版本修改而来，此版本稳定性及性能均有提升，感谢以下开源项目及其贡献者：

- 主项目：[crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader)
- 自选字体功能参考：[ruby-builds/crosspoint-reader (custom-fonts分支)](https://github.com/ruby-builds/crosspoint-reader/tree/feature/custom-fonts)
- 字体制作工具：[ZYFDroid/crosspointcn-fontcreator](https://github.com/ZYFDroid/crosspointcn-fontcreator)

---

# 当前进度

- **EPUB**：基本完成中文化适配
- **XTC**：实现动态管理功能
- **TXT**：目录解析逻辑如下：
  - 优先按“第n章”格式提取目录
  - 若无匹配目录或提取失败，则自动启用按字节分卷的兜底方案
  - *注：这部分是在原项目更新前独立编写的，刚写完就发现更新了txt hhh*

---

# 字体说明

## 内置字体
项目最初为满足个人阅读需求，内置字体选用 **汉仪空山楷**。  


## 自选字体

用户有两种方式制作字体：

python用户：选用usetool/文件夹下的test_gui_v1.py进行字体转换


Windows用户：选用[ZYFDroid/crosspointcn-fontcreator](https://github.com/ZYFDroid/crosspointcn-fontcreator)字体制作工具，制作后的字体在放置exe的文件夹下

转换完后放入fonts/文件夹中，具体可见：[ruby-builds/crosspoint-reader (custom-fonts分支)](https://github.com/ruby-builds/crosspoint-reader/tree/feature/custom-fonts)

格式：

1.字体名称.epdfont（无法调整大小，生成多大就是多大）
2.具体教程下一版出--按照这种格式明明字体：Family_Style_Size.epdfont（例如，Aileron_Regular_18.epdfont),生成一套字体可达成最佳效果


## 刷机指导


1. 需要一根typec线连接你的电脑和x4
2. 下载release页面下的bin文件
3. 打开 https://xteink.dve.al/ 页面，在OTA fast flash controls部分选择下载好的bin文件，点击flash firmware from file
4. 先短按reset(sd卡附近），再长按电源键

首次刷机建议做好保存，在full flash controls界面下，选择save full flash，备份一下你的官方固件

# 文件管理

适配新0.16.0版本，下方按键选择阅读历史和本地文件，侧边按键进行选择

# wifi传书
按屏幕提示来即可

# OPDS传书
与calibre同步，到时候会出教程

# 透明字体
需要工具自制，稍等更新

# 主页
主页自动生成封面，所以返回主页的时候时间会比较长，属于正常现象

# bugs
TXT：txt进入后需要手动翻页刷新，翻页略有卡顿，需要包容一下

# 问题解决
万事先重启

重启解决不了的，拔出sd卡，删除根目录下的.crosspoint文件夹

# 联系方式
可在小红书(4292493604)加群交流，也可以发邮件到gdby_mail@163.com,当然，最简单的是直接在github项目的讨论里发表看法

