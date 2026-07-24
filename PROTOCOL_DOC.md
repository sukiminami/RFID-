          












UHF RFID通信协议

读写器通讯协议
适用于所有读无源标签读头

文档版本： 4.0.3
发布日期： 2021-06-18






1.通信帧格式介绍
1.1.帧格式定义
帧是主机和RFID读写器传输数据的一个数据单元，所有的交互都以帧为一个单位
Header	Frame type	Address	Frame Code	Param Length	Parameters	Checksum
‘R’ ‘F’	1 byte	MSB LSB	1 byte	MSB     LSB	N bytes	1byte
Header:数据帧的起始标志，由字符’R’和字符’F’组成
Frame type:帧的类型，指明这个数据帧是命令，响应或者是通知类型的数据帧。
命令帧：值为0，命令帧通常用于上层控制设备向RFID读写器发送命令以控制读写，查询/设置参数等功能。
响应帧：值为1，RFID读写器在执行完主机的命令后发送响应告知执行的结果。
通知帧：值为2，RFID设备在没有接收到主机命令的情况下主动发送数据给主机，例如RFID读写器主动读卡模式下读到数据后自动上传给主机。
Address：RFID的设备地址，由2个字节组成，MSB在帧的前面，LSB在帧的后面。作为设备的一种标识，命令帧中只有地址和RFID读写器设备内的地址一致时设备才会响应，否则地址不一致的情况下RFID读写器不会执行主机的命令 。
Frame Code：帧的识别码，命令帧中Frame Code指明此命令帧是什么命令，响应帧则指明是对什么命令的响应。
Param Length：帧的参数长度(N bytes的中的N)， 由两个字节组成，MSB字节在前面，LSB在帧的后面
Parameters：数据帧的参数，参数的数据类型均以TLV的格式来表示
TLV：标签(Tag)，长度(Length)，值(Value)。除了基础的TLV，其余的TLV均可嵌套。
Checksum：帧的校验码，保证数据帧的完整性。从Header开始计算，一直到Checksum的前一个字节结束，如果主机和RFID读写器计算的checksum不一致则直接丢弃该帧。



1.2.校验和的计算方式
RFID_UINT8 caculate_checksum(RFID_UINT8 *buff_ptr,RFID_UINT8 len)
{
    RFID_UINT8 index = 0;
    RFID_UINT8 check_sum = 0;
    for (index = 0; index < len ; index++)
    {
        check_sum += buff_ptr[index];
    }
    check_sum = ~check_sum + 1;
    return check_sum;
}
















2.帧详细介绍
2.1查询设备软件版本等信息(0x40)
用于查询设备的固件版本号，设备类型
Header	Frame type	Address	Frame Code	Param Length	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x40	0x00     0x00	1byte
如: 52 46 00 00 00 40 00 00 28

响应格式如下：
Header	Frame type	Address	Frame Code	Param Length	Status tlv	Sfotware version
TLV	Device Type TLV	Checksum
‘R’ ‘F’	0x01	MSB LSB	0x40	MSB  LSB				1byte
如: 52 46 01 00 00 40 00 0B 07 01 00 20 03 04 00 01 21 01 05 C5
Software Version TLV 和 Device Type TLV将在后面给出详细介绍


2.2开始盘存标签(0x21)
开始盘存标签：命令设备开始持续读取标签直到接收到停止命令才停止。
Header	Frame type	Address	Frame Code	Param Length	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x21	0x00     0x00	1byte
响应格式：
Header	Frame type	Address	Frame Code	Param Length	Status tlv	Checksum
‘R’ ‘F’	0x01	MSB LSB	0x21	MSB LSB		1byte
Status TLV指示命令是否执行成功。
Host  Reader: 52 46 00 00 00 21 00 00 47
Host  Reader: 52 46 01 00 00 21 00 03 07 01 00 3B


2.3主动盘存标签(0x22)
在某些情况我们不希望设备一直盘存标签，而是发送一次命令设备读取一次，如果发送该命令设备在盘存一次标签后则会停止。
Header	Frame type	Address	Frame Code	Param Length	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x22	0x00     0x00	1byte
响应格式：格式请参考3.1标签的上传
Host  Reader: 52 46 00 00 00 22 00 00 46

2.4停止盘存标签(0x23)
通知设备停止盘存标签。
Header	Frame type	Address	Frame Code	Param Length	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x23	0x00     0x00	1byte
响应格式：
Header	Frame type	Address	Frame Code	Param Length	Status tlv	Checksum
‘R’ ‘F’	0x01	MSB LSB	0x23	MSB LSB		1byte
Status TLV指示命令是否执行成功。
Host  Reader: 52 46 00 00 00 23 00 00 45
Host  Reader: 52 46 01 00 00 23 00 03 07 01 00 39

2.5设置单个参数(0x48)
Header	Frame type	Address	Frame Code	Param Length	TLV	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x48	Length of tlv	Single Parameter tlv	1byte
如: 52 46 00 00 00 48 00 05 26 03 01 09 C4 24
26 02 01 1E
	26:单个参数的TLV类型值
	02:长度为2个字节
	01:此参数的类型为功率
	1E:功率的值,0x1E的十进制的值是30,则表示功率的值是30dbm
响应格式：
Header	Frame type	Address	Frame Code	Param Length	Status TLV	Checksum
‘R’ ‘F’	0x01	MSB LSB	0x48	0x00 0x03	0x07 0x01 0x00	1byte
状态码:0x00表示成功
2.6查询单个参数值(0x49)
Header	Frame type	Address	Frame Code	Param Length	TLV	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x49	Length of tlv	Single Parameter tlv	1byte
如: 52 46 00 00 00 49 00 03 26 01 01 F4
26 01 01:查询单个参数，这个参数的类型为0x01(功率)
回的响应如下:
Header	Frame type	Address	Frame Code	Param Length	Status TLV	TLV	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x49	Length of tlv	stauts	Single Parameter tlv	1byte
52 46 00 00 00 49 00 08 07 01 00 26 03 01 09 C4 18
07 01 00: status
26 03 01 09 C4: power TLV

2.7设置工作参数
Header	Frame type	Address	Frame Code	Param Length	TLV	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x41	Length of tlv	Working TLV	1byte

Working Parameter TLV:
TLV Value	Tlv len	version	RF Power	Inventory interval Time	Work mode	Inventory Membank	Inventory start addr	Inventory length
0x23	0x0F	0x05	1 byte	1byte	1byte	1byte	1byte	1byte
Filter Time	Device addr MSB	Device addr LSB	Beep Switch	Record Flag	Trigger Time	Attenna Flag MSB	Attenna Flag LSB	
1byte	1byte	1byte	1byte	1byte	1byte	1byte	1byte	

Version:0x05
RF Power: (0~30)射频功率，值越高读卡距离越远。
Inventory interval Time: (1~255 )盘寻间隔时间，读卡周期，单位:10ms。
Work Mode：(0~2)读卡器的工作模式。
			0:主动模式，上电后读写器不盘寻，在接收到上位机指令盘寻一次后停止。
			1:被动模式，上电后自动盘寻标签，接收到停止指令后停止，接收到开始盘寻指令后会一直盘寻
			2:触发模式，只有触发线上有触发信号时才会读卡
Inventory Membank:盘寻区域，0：Reserve 1:EPC 2:TID 3:User
Inventory start addr:盘寻区域的起始地址
Inventory Length:盘寻的长度。
默认情况下盘寻EPC，地址为0，长度为0。
Filter Time: (0~255)标签过滤时间，在多长时间内读到该标签不再上传，单位:秒
Device Addr:设备的地址，485下有效
Beep Switch:蜂鸣器开关，0:关闭  非零:开启
Record Flag:记录标签标志，目前不支持该功能
Attenna Flag:工作天线，0x0001代表只有1号天线工作，默认情况下为0.
例如：
52 46 00 00 00 41 00 11 23 0F 05 14 06 01 01 00 00 00 00 00 01 00 01 00 00 C1
响应：
Header	Frame type	Address	Frame Code	Param Length	Status tlv	Checksum
‘R’ ‘F’	0x01	MSB LSB	0x41	MSB LSB		1byte
52 46 01 00 00 41 00 03 07 01 00 1B

2.8重启设备(0x10)
设备重启。
Header	Frame type	Address	Frame Code	Param Length	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x10	0x00     0x00	1byte
响应格式：
Header	Frame type	Address	Frame Code	Param Length	Status tlv	Checksum
‘R’ ‘F’	0x01	MSB LSB	0x10	MSB LSB		1byte
Status TLV指示命令是否执行成功。
Host  Reader: 52 46 00 00 00 10 00 00 58
Host  Reader: 52 46 01 00 00 10 00 03 07 01 00 4C




2.9写标签(0x30)
Header	Frame type	Address	Frame Code	Parameters Len	parameter	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x30	XX XX	Operation TLV	1byte
写标签响应格式：
Header	Frame type	Address	Frame Code	Parameters Len	parameter	Checksum
‘R’ ‘F’	0x01	MSB LSB	0x30	XX XX	Status TLV	1byte

2.10读标签(0x31)
Header	Frame type	Address	Frame Code	Parameters Len	parameter	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x31	XX XX	Operation TLV	1byte
响应格式:
Header	Frame type	Address	Frame Code	Parameters Len	parameter	parameter	Checksum
‘R’ ‘F’	0x01	MSB LSB	0x31	XX XX	Status TLV	Operation TLV	1byte

2.11继电器控制（0x4C）
说明：用于控制继电器的打开或者关闭。
Header	Frame type	Address	Frame Code	Parameters Len	parameter	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x4C	XX XX	Relay TLV	Relay TLV	1byte
参数中可以有多个Relay的TLV。
Relay TLV:
Attribute Code	Attribute len	Relay No	Operation	Time
0x27	0x03	1byte	1byte	1byte
Relay No:要操作的继电器号，目前设备支持1,2号两个继电器。
Operation:打开或者关闭继电器。0：关闭继电器   非零：打开继电器
Time:打开继电器的时间，以秒为单位，达到时间后自动关闭继电器。如果是0则表示长期打开继电器需手动关闭。

响应格式：
Header	Frame type	Address	Frame Code	Parameters Len	parameter	Checksum
‘R’ ‘F’	0x01	MSB LSB	0x4C	XX XX	Status TLV	1byte



2.12语音播放（0x4D）
说明：用于控制继电器的打开或者关闭。
Header	Frame type	Address	Frame Code	Parameters Len	parameter	Checksum
‘R’ ‘F’	0x00	MSB LSB	0x4D	XX XX	Audio TLV	1byte
参数中可以有多个Relay的TLV。
Audio TLV:
Attribute Code	Attribute len	Operation	Text
0x28	1byte	1byte	N byte
Operation:要执行的语音操作。
0x01: 播放语音文本信息 
0x02：设置离线语音内容
Text:要播放的语音信息内容，以GBK的编码方式下发，否则无法播放正确的语音信息。如果是非语音播放内容则代表音量值，声音类型或者播放速度。

响应格式：
Header	Frame type	Address	Frame Code	Parameters Len	parameter	Checksum
‘R’ ‘F’	0x01	MSB LSB	0x4	XX XX	Status TLV	1byte






3.提示帧介绍
目前只要用于读写器在没有接收到命令的情况下通知设备的一种消息机制，例如读取到标签后上传标签数据。
响应帧的格式如下：
Header	Frame type	Address	Frame Code	Param Length	Parameters	Checksum
‘R’ ‘F’	0x02	MSB LSB	1 byte	MSB     LSB	N bytes	1byte

3.1 标签的上传
Header	Frame type	Address	Frame Code	Param Length	TLV	Checksum
‘R’ ‘F’	0x02	MSB LSB	0x80	Length of tlv	Tag TLV	1byte
如接收到一下数据：
52 46 02 00 00 80 00 19 50 17 01 0C E2 00 00 17 02 17 01 99 23 90 21 7D 05 01 C3 06 04 3D 00 00 00 4C
50(hex):Single Tag TLVd的起始位置，其长度为0x17
EPC TLV :01 0C E2 00 00 17 02 17 01 99 23 90 21 7D (绿色为标签的数据)
RSSI:05 01 C3（非所有型号都带）
Time TLV:06 04 3D 00 00 00（非所有型号都带）

3.2 离线标签上传
Header	Frame type	Address	Frame Code	Param Length	TLV	Checksum
‘R’ ‘F’	0x02	MSB LSB	0x81	Length of tlv	Tag TLV	1byte
如接收到一下数据：
52 46 02 00 00 81 00 19 50 17 01 0C E2 00 00 17 02 17 01 99 23 90 21 7D 06 07 07 E5 08 0C 08 1E 0A A8
50(hex):Single Tag TLVd的起始位置，其长度为0x17
EPC TLV :01 0C E2 00 00 17 02 17 01 99 23 90 21 7D (绿色为标签的数据)
Time TLV: 06 07 07 E5 08 0C 08 1E 0A（非所有型号都带,代表2021年8月12日8:30:10）


4.TLV介绍
交互数据中的参数和返回值均以TLV的方式表示，其格式如下：
Attribute Type	Attribute Value Length	Attribute Value
    1 byte	1byte 	N bytes


4.1 Status
主要用于返回命令的执行结果
Attribute Type	Length	Value
0x07	0x01	Status code
Status Code的值意义如下:

序号		值	名 称	描    述
1		0x00	SUCCESS	命令成功完成
2		0x14	Parameter unsupport	不支持的参数，例如在设置单个参数里填写了一个不支持设置的参数类型
3		0x15	Parameter len error	参数的长度填写有误
4		0x16	Parameter context error	填写的参数内容有误
5		0x17	Unsupport command	不支持的命令
6		0x18	Device Address error	命令中的设备地址和命令中的地址不符
7		0x20	Check Sum error	校验码错误
8		0x21	Unsupport TLV Type	设备内部错误
9		0x22	Flash Error	存储参数时写入flash错误
10		0xFF	Internal Error	内部错误
4.2 Software Version
Attribute Type	Attribute Length	Attribute Value
0x20	0x03	Main Vsersion, Sub Version,Modify Version
如数据: 20 03 04 00 01, 固件的版本号为:4.0.1

4.3 Device Type
用于标记设备的类型
Attribute Type	Attribute Length	Attribute Value
0x21	0x03	1 byte(device type)



4.4 Single Parameter
表示在设置单个参数中的值
Attribute Type	Attribute Length	Attribute Value
0x26	Length	1 byte(paraemter type)	Parameter value
Parameter type:要设置的参数类型
Parameter Type	Parameter Value	说明
0x01	1byte	设置功率，最大值30，表示30dbm
0x02	1 byte	0：关闭蜂鸣器  1:开启蜂鸣器
0x03	1 byte	标签过滤时间，1~255，单位为s
0x04	Mixer Gain	IF AMP Gain	MSB Threshold	LSB Threshold	Mixer Gain:默认为9
IF AMP Gain:默认为36
Threshold:信号门槛，值越高读卡距离越近，默认值为0x00A0
例如设置功率: 52 46 00 00 00 48 00 05 26 03 01 1E checksum     1E转换十进制的值为30,则该命令就是将功率设置为30dbm.
开启蜂鸣器:52 46 00 00 00 48 00 04 26 02 02 01 F1     01说明要开启蜂鸣器,00则关闭蜂鸣器
设置modem参数：52 46 00 00 00 48 00 07 26 05 04 09 24 00 A0 1D   mixer gain:0x09  IF AMP Gain: 0x24 threshold:0x00A0
4.5 EPC TLV
Attribute Type	Attribute Length	Attribute Value
0x01	length	EPC 数据
由于EPC数据的长度是可以修改的，因此length的值不固定
4.6 RSSI
Attribute Type	Attribute Length	Attribute Value
0x05	0x01	RSSI

4.6 Time
Attribute Type	Attribute Length	Year	Month	day	hour	minute	second
0x06	0x07	2bytes	1byte	1byte	1byte	1byte	1byte

4.5 Tag TLV
单张标签的数据格式，该数据格式可以包含EPC数据，RSSI，时间戳，TID等之类的数据，不一定每个TLV属性都有，可能只有EPC TLV 或者TID TLV
Attribute Type	Attribute Length	Attribute Value
0x50	length	[EPC TLV] [RSSI TLV] [Time TLV] [TID TLV] …….
如接收到一下数据：
52 46 02 00 00 80 00 19 50 17 01 0C E2 00 00 17 02 17 01 99 23 90 21 7D 05 01 C3 06 04 3D 00 00 00 4C
50(hex):Single Tag TLVd的起始位置，其长度为0x17
EPC TLV :01 0C E2 00 00 17 02 17 01 99 23 90 21 7D (绿色为标签的数据)
RSSI:05 01 C3
Time TLV:06 04 3D 00 00 00



4.6 Operation TLV
用于对标签进行操作时所传输入的参数
Attribute Type	Attribute Length	Password	type	membank	address	length	[data]
0x08	1 byte	4 bytes	1 byte	1 byte	1 byte	 1 byte	[(Length * 2) bytes]
Password: 在标签的Reserve区存储四个字节的密码，在对标签进行操作的时候只有密码一致才有权限进行读，修改，锁定，销毁等操作。Reserve区的地址为0的前4个字[00H ~ 1FH]存储Access密码，后4个字[20H ~ 3FH]存储Kill即销毁标签的密码，若密码都是0则销毁标签命令无效。

Type: 操作的类型，其值和说明如下：
	
值	说明	
0	读标签	
1	写标签	
2	锁定标签	
3	销毁标签	
		
		

[读标签或者写标签]
Membank: 标签的区域，标签中有四个区域，分别为Reserve, EPC, TID, User区域。
Membank	
0x00	Reserver
0x01	EPC
0x02	TID
0x03	User

	Reserve:存储操作标签所需的密码
EPC: 产品电子代码，以0为起始的前4个字节为保留字段，如果不太了解请不要修改。在盘存时读取到的EPC     数据是从地址为2的位置开始。
Address: 在进行读或者写操作时指定从Membank中的哪个位置作为操作的起始地址
Length: 操作的长度，以字为单位，即2个字节为1个单位。
Data:在作为写操作时作为要写入的内容,如果操作是读的响应则为读取到的内容。

[锁定标签操作]

