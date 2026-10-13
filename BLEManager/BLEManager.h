//
//  BLEManager.h
//  HYHBleSDK
//
//  Created by hyh on 22/1/15.
//

#import <Foundation/Foundation.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import "CBPeripheral+Peripherals.h"


typedef NS_ENUM(NSInteger, BleError) {
    BleErrorBluetoothDisable = 100, // 蓝牙未打开
    BleErrorBluetoothUnsupported, // 设备不支持蓝牙
    BleErrorBluetoothUnauthorized, // 蓝牙未授权
    BleErrorReadCharacteristicNil = 200, // 读特征值为空
    BleErrorWriteCharacteristicNil, // 写特征值为空
    BleErrorSetNotifyingFailed, // 设置监听失败
    BleErrorReceiveDataTimeOut, // 接收数据等待超时
};

@protocol BLEManagerDelegate <NSObject>

@optional

/// 蓝牙状态改变
- (void)bleStateChanged:(CBManagerState )state;

/// 已搜索到新BLE
-(void)didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData name:(NSString *)name RSSI:(NSNumber *)RSSI;

/// 开始连接设备
- (void)showWaitingView;

/// 已连接BLE
-(void)didConnectedPeripheral:(CBPeripheral *)peripheral;

/// 已断开连接BLE
-(void)disConnectedPeripheral:(CBPeripheral *)peripheral;

/// 搜索到BLE服务
-(void)didDiscoverServiceForPeripheral:(CBPeripheral *)peripheral;

/// 搜索到BLE特征
-(void)didDiscoverCharacterForPeripheral:(CBPeripheral *)peripheral;

/// 已设置监听BLE
-(void)didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error peripheral:(CBPeripheral *)peripheral;

/// 已读取到BLE的RSSI
- (void)didReadRSSI:(NSNumber *)rssi;

/// 已接收到BLE数据
-(void)didReceiveDataForCharacter:(CBCharacteristic *)characteristic;

/// 数据写入BLE已完成
-(void)didSendDataForCharacter:(CBCharacteristic *)characteristic;

/// 连接超时
-(void)homeConnectTimeOut:(NSString *)bleName;

/// 蓝牙出错
- (void)BLEManagerError:(BleError)bleError;

@end



@interface BLEManager : NSObject

@property (nonatomic, weak) id<BLEManagerDelegate> delegate;
/// 服务特征值
@property (nonatomic, strong) NSString *serviceCharecteristic;
/// 写特征值
@property (nonatomic, strong) NSString *writeCharecteristic;
/// 读特征值
@property (nonatomic, strong) NSString *readCharecteristic;
/// 最新连接的设备
@property (nonatomic, strong) CBPeripheral *curPeripheral;
/// 保存已经连接的设备
@property (nonatomic, strong) NSMutableArray *connectedPeripherals;
/// 保存扫描到的设备
@property (nonatomic, strong) NSMutableArray *scannedPeripherals;
/// 保存需要自动连接的设备的UUID
@property (nonatomic, strong) NSMutableArray *autoConnectUUIDs;
/// 蓝牙是否已打开
//@property (nonatomic, assign,  getter=isEnableBLE) BOOL enableBLE;
/// BLE是否已连接设备
@property (nonatomic, assign, getter=isBLEConnected) BOOL BLEConnected;
/// 是否显示名字为nil的蓝牙设备，默认为NO
@property (nonatomic, assign, getter=isShowNilNameDevice) bool showNilNameDevice;
/// 是否读取到蓝牙数据
@property (nonatomic, assign, getter=isReadData) BOOL readData;
/// 收到的蓝牙数据（Byte *）
@property (nonatomic, assign) Byte *receivedBytes;
/// 收到的蓝牙数据（NSData）
@property (nonatomic, strong) NSData *receivedData;
/// 筛选特定名字的蓝牙设备，除了这些名字外都不显示
@property (nonatomic, strong) NSMutableArray *filterBLENameArray;



/// 单例方法
+ (BLEManager *)share;

/// 扫描外围设备
- (void)startScan;

/// 扫描指定的外围设备
/// - Parameter serviceUUIDs: service Charecteristic数组
- (void)startScanWithServices:(NSArray<CBUUID *> *)serviceUUIDs;

/// 停止扫描BLE
- (void)stopScan;

/// 连接设备
- (void)connectPeripheral:(CBPeripheral *)peripheral;

/// 连接指定的设备
- (void)connectWithBLEUUID:(NSString *)bleUUID;

/// 指定UUID断开连接BLE
- (void)disConnectPeripheraltWithBLEUUID:(NSString *)bleUUID;

/// 断开设备
- (void)disConnectPeripheral:(CBPeripheral *)peripheral;

/// 断开所有设备
- (void)disConnectAllPeripheral;

#pragma mark -

/// 写入字符串数据到当前蓝牙设备
/// - Parameter text: 字符串数据，格式为十六进制并且用空格分开，e.g. “fe 1a 25 08 ”
- (void)sendTextDataToCurperipheral:(NSString *)text;

/// 写入字符串数据到指定蓝牙设备
/// - Parameters:
///   - text: 字符串数据，格式为十六进制并且用空格分开，e.g. “fe 1a 25 08 ”
///   - peripheral: 蓝牙设备
- (void)sendTextData:(NSString *)text toPeripheral:(CBPeripheral *)peripheral;

/// 写入字符串数据到所有的蓝牙设备
/// - Parameter text: 字符串数据，格式为十六进制并且用空格分开，e.g. “fe 1a 25 08 ”
- (void)sendTextDataToAllPeripheral:(NSString *)text;

/// 写入Byte数据到蓝牙设备
/// - Parameters:
///   - byteData: Byte数据
///   - length: 数据长度
///   - peripheral: 目标蓝牙设备
- (void)sendBytes:(Byte *)byteData length:(NSInteger)length toPeripheral:(CBPeripheral *)peripheral;

/// 写入NSData数据到蓝牙数据
/// - Parameters:
///   - data: NSData数据
///   - peripheral: 目标蓝牙设备
-(void)sendNSData:(NSData *)data toPeripheral:(CBPeripheral *)peripheral;

/// 等待收到当前设备的数据
- (bool)checkAckFromCurPeripheral;

/// 等待收到指定设备的数据
- (bool)checkAckFromPeripheral:(CBPeripheral *)peripheral;


@end

