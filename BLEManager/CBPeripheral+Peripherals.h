//
//  CBPeripheral+Peripherals.h
//  BLEDemo
//
//  Created by hyh on 25/1/2021.
//

#import <CoreBluetooth/CoreBluetooth.h>


@interface CBPeripheral (Peripherals)

@property (nonatomic, copy) NSString *rename;           // 重命名
@property (nonatomic, copy) NSString *writable;         // 是否可写(不能用BOOL)
@property (nonatomic, copy) NSString *connected;        // 是否已连接

@property (nonatomic, retain) NSNumber *curRSSI;        // 当前RSSI

@property (nonatomic, retain) CBCharacteristic *readCharacteristic;     // 读特征
@property (nonatomic, retain) CBCharacteristic *writeCharacteristic;    // 写特征
@property (nonatomic, copy) NSString *localName;    // 广播包的蓝牙名字

@end

