//
//  BLEManager.m
//  HYHBleSDK
//
//  Created by hyh on 22/1/15.
//

#import "BLEManager.h"

@interface BLEManager ()<CBCentralManagerDelegate,CBPeripheralDelegate>
@property (nonatomic,strong)CBCentralManager *bmgr; //中央管理者
@property (nonatomic,strong)NSTimer *connectTimer;      //连接计时

@end

@implementation BLEManager

//单例
static BLEManager *bleManager = nil;
+ (BLEManager *)share{
    
    //类似线程锁，保证block中的代码在整个程序运行当中只执行一次
    static dispatch_once_t oneToToken;
    //oneToToken: 检查后面的代码块是否被调用
    dispatch_once(&oneToToken, ^{
        if(!bleManager){
            bleManager = [BLEManager new];
            //设置默认特征值
            bleManager.serviceCharecteristic = @"FFF0";
            bleManager.readCharecteristic = @"FFF1";
            bleManager.writeCharecteristic = @"FFF2";
        }
    });
    return bleManager;
}


//初始化
- (instancetype)init
{
    self = [super init];
    if (self) {
        self.bmgr = [[CBCentralManager alloc]initWithDelegate:self queue:nil];
    }
    return self;
}


#pragma mark- CorerBluetooth代理方法

// 状态发生改变时会执行该方法(蓝牙4.0没有打开变成打开时调用)
- (void)centralManagerDidUpdateState:(CBCentralManager *)central{
    switch (central.state) {
        case CBManagerStatePoweredOn: {
            NSLog(@"----系统蓝牙打开");
        } break;
            
        case CBManagerStatePoweredOff: {
            NSLog(@"----⚠️系统蓝牙关闭");
            [self throwError:BleErrorBluetoothDisable];
            [self.connectedPeripherals removeAllObjects];
            self.BLEConnected = NO;
        } break;
            
        case CBManagerStateUnsupported: {
            NSLog(@"----⚠️系统不支持蓝牙");
            [self throwError:BleErrorBluetoothUnsupported];
        } break;
            
        case CBManagerStateUnauthorized: {
            NSLog(@"----⚠️系统蓝牙未授权");
            [self throwError:BleErrorBluetoothUnauthorized];
        } break;
            
        default:
            break;
    }
    if ([self.delegate respondsToSelector:@selector(bleStateChanged:)]) {
        [self.delegate bleStateChanged:central.state];
    }
    
}

//当发现外围设备时,会调用该方法
//参数一: 发现的外围设备
//参数二: 外围设备发出信号
//参数三: 信号强度
- (void)centralManager:(CBCentralManager *)central didDiscoverPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData RSSI:(NSNumber *)RSSI{
    
    if(peripheral == nil){
        NSLog(@"Find nil device!");
        return ;
    }
    // 更新信号强度
    peripheral.curRSSI = RSSI;
    
    NSString *localName = [advertisementData objectForKey:@"kCBAdvDataLocalName"];
    if (localName) {
        peripheral.localName = [NSMutableString stringWithString:localName];
    }

    //是否已经包含了该外围设备
    if (![self.connectedPeripherals containsObject:peripheral] && ![self.scannedPeripherals containsObject:peripheral]) {

        // 检查该设备是否需要自动连接
        self.autoConnectUUIDs = [[[NSUserDefaults standardUserDefaults] objectForKey:@"keyAutoConnectArr"] mutableCopy];
        for (NSString *name in _autoConnectUUIDs) {
            if ([name isEqualToString:localName]) {
                // 开一个定时器处理连接超时
                self.connectTimer = [NSTimer scheduledTimerWithTimeInterval:5.0
                                                                     target:self
                                                                   selector:@selector(connectTimeOut:)
                                                                   userInfo:@{@"deviceBLEUUID":name}
                                                                    repeats:NO];
                // 连接外围设备
                [self.bmgr connectPeripheral:peripheral options:nil];
                 //回调通知，调出等待界面
                if ([self.delegate respondsToSelector:@selector(showWaitingView)]) {
                    [self.delegate showWaitingView];
                }
            }
        }

    }
    
    // 是否显示名字为nil的蓝牙设备
    if (self.isShowNilNameDevice) {
        // 添加蓝牙设备
        [self addToDisPeripheral:peripheral advertisementData:advertisementData];
    } else {
        if (localName != nil) {
            // 筛选特定名字的蓝牙设备
            if (self.filterBLENameArray.count > 0) {
                for (NSString *name in self.filterBLENameArray) {
                    if ([peripheral.localName containsString:name]) {
                        // 添加蓝牙设备
                        [self addToDisPeripheral:peripheral advertisementData:advertisementData];
                    }
                }
            } else {
                // 添加蓝牙设备
                [self addToDisPeripheral:peripheral advertisementData:advertisementData];
            }
        }
    }
    
}

// 连接上外围设备的时候会调用该方法
-(void)centralManager:(CBCentralManager *)central didConnectPeripheral:(CBPeripheral *)peripheral{
    self.BLEConnected = YES;
    self.curPeripheral = peripheral;
    peripheral.connected = @"YES";
    peripheral.writable = @"YES";       // 默认可写

    [self.connectTimer invalidate];
    self.connectTimer = nil;

    // 添加需要自动连接设备的UUID到本地
    self.autoConnectUUIDs = [[[NSUserDefaults standardUserDefaults] objectForKey:@"keyAutoConnectArr"] mutableCopy];
    if (![self.autoConnectUUIDs containsObject:peripheral.localName]) {
        [self.autoConnectUUIDs addObject:peripheral.localName];
    }
    [[NSUserDefaults standardUserDefaults] setObject:self.autoConnectUUIDs forKey:@"keyAutoConnectArr"];
    
    // connectedPeripherals添加设备
    [self.connectedPeripherals addObject:peripheral];
    // scannedPeripherals移除设备
    [self.scannedPeripherals removeObject:peripheral];

    // 代理回调
    if([_delegate respondsToSelector:@selector(didConnectedPeripheral:)]){
        [_delegate didConnectedPeripheral:peripheral];
    }

    // 扫描设备所有的服务,可以指定扫描该外围设备的哪些服务
    [peripheral discoverServices:nil];
    // 设置代理
    peripheral.delegate = self;
}

/// 断开所有连接
- (void)centralManager:(CBCentralManager *)central didDisconnectPeripheral:(CBPeripheral *)peripheral error:(nullable NSError *)error{
    
    NSLog(@"----蓝牙已断开");
    peripheral.connected = @"NO";
    self.curPeripheral = nil;
    
    // connectedPeripherals移除该设备
    [self.connectedPeripherals removeObject:peripheral];
    if ([self.connectedPeripherals count] == 0) {
        self.BLEConnected = NO;
    }
    [self.scannedPeripherals removeObject:peripheral];
    
    if([self.delegate respondsToSelector:@selector(disConnectedPeripheral:)]){
        [_delegate disConnectedPeripheral:peripheral];
    }
    
    // 开始扫描
    [self.bmgr scanForPeripheralsWithServices:nil options:nil];
}


#pragma mark-CBPeripheral的代理方法

// 发现服务
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverServices:(NSError *)error{
    
    if (error) {
        NSLog(@"-->Discovered services for %@ with error: %@", peripheral.name, [error localizedDescription]);
        return;
    }
    
    for(CBService *service in peripheral.services) {
        NSLog(@"----Found Service: %@", service.description);
        
        //可以指定要扫描的特征,传nil为扫描所有特征
        if ([service.UUID.UUIDString containsString:self.serviceCharecteristic]) {
            NSLog(@"Service found with UUID:%@",service.UUID);
            //可以指定要扫描的特征,传nil为扫描所有特征
            [peripheral discoverCharacteristics:nil forService:service];
        }
    }
}


// 发现特征
- (void)peripheral:(CBPeripheral *)peripheral didDiscoverCharacteristicsForService:(CBService *)service error:(NSError *)error{
    
    if (error){
        NSLog(@"-->Error:Discovered characteristics for %@ with error: %@", service.UUID, [error localizedDescription]);
        return;
    }
    
//    NSMutableArray *array = [NSMutableArray array];
//    //轮询特征值
//    for(CBCharacteristic *characteristic in service.characteristics){
//        NSLog(@"---->Discovered characteristics:%@ for service:%@",characteristic.description, service.description);
//        [array addObject:characteristic];
//        if ([characteristic.UUID.UUIDString isEqualToString:self.curReadCharecterisitic]) {
//            //设置通知
//            [peripheral setNotifyValue:YES forCharacteristic:characteristic];
//        }
//    }
    
    if([service.UUID.UUIDString containsString:self.serviceCharecteristic]) {
        for(CBCharacteristic *characteristic in service.characteristics) {
            NSLog(@"---->Discovered characteristics:%@ for service:%@",characteristic.UUID, service.UUID);

            NSLog(@"---%@", characteristic.UUID.UUIDString);
            if ([characteristic.UUID.UUIDString containsString:self.writeCharecteristic]) {
                //保存写的特征
                peripheral.writeCharacteristic = characteristic;
            }
            if ([characteristic.UUID.UUIDString containsString:self.readCharecteristic]) {
                //保存读的特征
                peripheral.readCharacteristic = characteristic;
                //设置通知
                [peripheral setNotifyValue:YES forCharacteristic:characteristic];
            }
        }
    }
    // 代理回调
    if ([self.delegate respondsToSelector:@selector(didDiscoverCharacterForPeripheral:)]) {
        [_delegate didDiscoverCharacterForPeripheral:peripheral];
    }
}

// 读通知
- (void)peripheral:(CBPeripheral *)peripheral didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error {
    
    // Exits if it's not the transfer characteristic
    NSLog(@"---%@", characteristic.UUID);
    if ([characteristic.UUID.UUIDString containsString:self.readCharecteristic] ) {
        if (characteristic.isNotifying) {
//            [peripheral readValueForCharacteristic:characteristic];
            NSLog(@"设置监听成功");
            if ([self.delegate respondsToSelector:@selector(didUpdateNotificationStateForCharacteristic:error:peripheral:)]) {
                [self.delegate didUpdateNotificationStateForCharacteristic:characteristic error:error peripheral:peripheral];
            }
        }else{
            NSLog(@"---⚠️设置监听失败");
            [self throwError:BleErrorSetNotifyingFailed];
        }
    }
}

// 读RSSI
- (void)peripheral:(CBPeripheral *)peripheral didReadRSSI:(NSNumber *)RSSI error:(nullable NSError *)error {
    
    if (error) {
        NSLog(@"Read RSSI error: %@", [error localizedDescription]);
        return;
    }
    
    peripheral.curRSSI = RSSI;
    
    if ([self.delegate respondsToSelector:@selector(didReadRSSI:)]) {
        [_delegate didReadRSSI:RSSI];
    }
}

// 写数据成功
- (void)peripheral:(CBPeripheral *)peripheral didWriteValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error{
    
    NSLog(@"---> characteristic:%@,send error:%@",characteristic,error);
    // 代理回调
    if([self.delegate respondsToSelector:@selector(didSendDataForCharacter:)]){
        [self.delegate didSendDataForCharacter:characteristic];
    }
}


// 读数据
- (void)peripheral:(CBPeripheral *)peripheral didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic error:(NSError *)error{
    
    if (error){
        NSLog(@"-->Error updating value for characteristic %@ error: %@", characteristic.UUID, [error localizedDescription]);
        return;
    }
    
    NSLog(@"---read：%@",characteristic.value);
    
    Byte *dataBytes = (Byte *)[characteristic.value bytes];
    if (dataBytes != nil) {
        self.readData = YES;
        self.receivedBytes = dataBytes;
        self.receivedData = characteristic.value;
        // 代理回调
        if([self.delegate respondsToSelector:@selector(didReceiveDataForCharacter:)]){
            [_delegate didReceiveDataForCharacter:characteristic];
        }
    }
    
}


#pragma mark - 数据交互
/// 写入字符串数据到当前蓝牙设备
/// - Parameter text: 字符串数据，格式为十六进制并且用空格分开，e.g. “fe 1a 25 08 ”
- (void)sendTextDataToCurperipheral:(NSString *)text {
    [self sendTextData:text toPeripheral:self.curPeripheral];
}

/// 写入字符串数据到指定蓝牙设备
/// - Parameters:
///   - text: 字符串数据，格式为十六进制并且用空格分开，e.g. “fe 1a 25 08 ”
///   - peripheral: 蓝牙设备
- (void)sendTextData:(NSString *)text toPeripheral:(CBPeripheral *)peripheral {
    if ([self.connectedPeripherals containsObject:peripheral]) {
        NSLog(@"---write: %@",text);
        NSArray *dataArr = [text componentsSeparatedByString:@" "];
        Byte sendData[dataArr.count];
        for(int i=0; i<dataArr.count; i++){
            //转成十六进制
            sendData[i] = strtoul([dataArr[i] UTF8String],0,16);
        }
        [self sendBytes:sendData length:dataArr.count toPeripheral:peripheral];
    }
}

/// 写入字符串数据到所有的蓝牙设备
/// - Parameter text: 字符串数据，格式为十六进制并且用空格分开，e.g. “fe 1a 25 08 ”
- (void)sendTextDataToAllPeripheral:(NSString *)text {
    // 遍历所有设备
    for (CBPeripheral *peripheral in self.connectedPeripherals) {
        if ([peripheral.writable isEqualToString:@"YES"]) {
            NSLog(@"---writeTo:%@, %@", peripheral.localName, text);
            NSArray *dataArr = [text componentsSeparatedByString:@" "];
            Byte sendData[dataArr.count];
            for(int i=0; i<dataArr.count; i++){
                //转成十六进制
                sendData[i] = strtoul([dataArr[i] UTF8String],0,16);
            }
            [self sendBytes:sendData length:dataArr.count toPeripheral:peripheral];
        }
    }
}

/// 写入Byte数据到蓝牙设备
/// - Parameters:
///   - byteData: Byte数据
///   - length: 数据长度
///   - peripheral: 目标蓝牙设备
- (void)sendBytes:(Byte *)byteData length:(NSInteger)length toPeripheral:(CBPeripheral *)peripheral {
    self.readData = NO;
    NSData *data = [[NSData alloc]initWithBytes:byteData length:length];
    [self sendNSData:data toPeripheral:peripheral];
}

/// 写入NSData数据到蓝牙数据
/// - Parameters:
///   - data: NSData数据
///   - peripheral: 目标蓝牙设备
-(void)sendNSData:(NSData *)data toPeripheral:(CBPeripheral *)peripheral {
    if ([self.connectedPeripherals containsObject:peripheral] == NO) {
        return;
    }
    self.readData = NO;
    
    // 判断特征值是否为空
    if (peripheral.writeCharacteristic) {
        if((peripheral.writeCharacteristic.properties & CBCharacteristicPropertyWriteWithoutResponse) == CBCharacteristicPropertyWriteWithoutResponse)
        {
            [peripheral writeValue:data forCharacteristic:peripheral.writeCharacteristic type:CBCharacteristicWriteWithoutResponse];
        }else
        {
            [peripheral writeValue:data forCharacteristic:peripheral.writeCharacteristic type:CBCharacteristicWriteWithResponse];
        }
    } else {
        NSLog(@"---⚠️writeCharacteristic = nil!");
        [self throwError:BleErrorWriteCharacteristicNil];
    }
}


#pragma mark -

/// 扫描外围设备
- (void)startScan {
    [self startScanWithServices:@[]];
}

/// 扫描指定的外围设备
/// - Parameter serviceUUIDs: service Charecteristic数组
- (void)startScanWithServices:(nullable NSArray<CBUUID *> *)serviceUUIDs {
    NSLog(@"----开始扫描蓝牙设备");
    [self.scannedPeripherals removeAllObjects];
    [self stopScan];
    
    if([self.bmgr state] == CBManagerStatePoweredOn) {
        if (serviceUUIDs.count > 0) {
            [self.bmgr scanForPeripheralsWithServices:serviceUUIDs options:@{CBCentralManagerScanOptionAllowDuplicatesKey:@(NO)}];
        } else {
            [self.bmgr scanForPeripheralsWithServices:nil options:@{CBCentralManagerScanOptionAllowDuplicatesKey:@(NO)}];
        }
    }else{
        NSLog(@"----⚠️蓝牙未打开");
        [self throwError:BleErrorBluetoothDisable];
    }
}


// 停止扫描
-(void)stopScan {
    [self.bmgr stopScan];
}

/// 连接设备
- (void)connectPeripheral:(CBPeripheral *)peripheral {
    [self connectWithBLEUUID:peripheral.identifier.UUIDString];
}

 /// 连接指定的设备
- (void)connectWithBLEUUID:(NSString *)bleUUID {
    
    // 开一个定时器，用来实现连接超时
    self.connectTimer = [NSTimer scheduledTimerWithTimeInterval:5.0
                                                         target:self selector:@selector(connectTimeOut:)
                                                       userInfo:@{@"deviceBLEUUID":bleUUID}
                                                        repeats:NO];
    for (CBPeripheral *peripheral in self.scannedPeripherals) {
        if ([peripheral.identifier.UUIDString isEqualToString:bleUUID]) {
            if (![peripheral.connected isEqualToString:@"YES"]) {
                // 连接BLE
                [self.bmgr connectPeripheral:peripheral options:nil];
                // 回调通知，调出等待界面
                if ([self.delegate respondsToSelector:@selector(showWaitingView)]) {
                    [self.delegate showWaitingView];
                }
            }
        }
    }
}

/// 检查连接是否超时
-(void)connectTimeOut:(NSTimer *)timer {

    NSString *UUIDString = [[timer userInfo] objectForKey:@"deviceBLEUUID"];
    
    //回调
    if([_delegate respondsToSelector:@selector(homeConnectTimeOut:)]){
        [_delegate homeConnectTimeOut:UUIDString];
    }
    
    [self.connectTimer invalidate];
    self.connectTimer = nil;
}

// 断开连接某个设备
- (void)disConnectPeripheraltWithBLEUUID:(NSString *)bleUUID {
    
    for(CBPeripheral *peripheral in self.connectedPeripherals){
        if([peripheral.identifier.UUIDString isEqualToString:bleUUID]){
            if ([peripheral.connected isEqualToString:@"YES"]) {
                
                // 从本地需要自动连接列表中移除该设备的UUID
                self.autoConnectUUIDs = [[[NSUserDefaults standardUserDefaults] objectForKey:@"keyAutoConnectArr"] mutableCopy];
                [self.autoConnectUUIDs removeObject:peripheral.localName];
                [[NSUserDefaults standardUserDefaults] setObject:[self.autoConnectUUIDs mutableCopy] forKey:@"keyAutoConnectArr"];
                
                // 断开连接
                [self.bmgr cancelPeripheralConnection:peripheral];
            }
        }
    }
}

// 断开设备
- (void)disConnectPeripheral:(CBPeripheral *)peripheral {
    for(CBPeripheral *targetPeripheral in self.connectedPeripherals){
        if([targetPeripheral isEqual:peripheral]){
            if ([targetPeripheral.connected isEqualToString:@"YES"]) {
                
                // 从本地需要自动连接列表中移除该设备的UUID
                self.autoConnectUUIDs = [[[NSUserDefaults standardUserDefaults] objectForKey:@"keyAutoConnectArr"] mutableCopy];
                [self.autoConnectUUIDs removeObject:targetPeripheral.localName];
                [[NSUserDefaults standardUserDefaults] setObject:[self.autoConnectUUIDs mutableCopy] forKey:@"keyAutoConnectArr"];
                
                // 断开连接
                [self.bmgr cancelPeripheralConnection:targetPeripheral];
            }
        }
    }
}

// 断开所有设备
- (void)disConnectAllPeripheral {
    
    if ([self.connectedPeripherals count]) {
        for (CBPeripheral *peripheral in self.connectedPeripherals) {
            if ([peripheral.connected isEqualToString:@"YES"]) {
                
                // 从本地需要自动连接列表中移除该设备的UUID
                self.autoConnectUUIDs = [[[NSUserDefaults standardUserDefaults] objectForKey:@"keyAutoConnectArr"] mutableCopy];
                [self.autoConnectUUIDs removeObject:peripheral.name];
                [[NSUserDefaults standardUserDefaults] setObject:[self.autoConnectUUIDs copy] forKey:@"keyAutoConnectArr"];
                
                // 断开连接
                [self.bmgr cancelPeripheralConnection:peripheral];
            }
        }
    }
}

/// 等待收到当前设备的数据
- (bool)checkAckFromCurPeripheral {
    return [self checkAckFromPeripheral:self.curPeripheral];
}

/// 等待收到指定设备的数据
- (bool)checkAckFromPeripheral:(CBPeripheral *)peripheral {
    if ([self.connectedPeripherals containsObject:peripheral] == NO) {
        return NO;
    }
    NSLog(@"----checkAckFromPeripheral");
    NSInteger count = 0;
    
    //判断是否在主线程里，涉及到循环等待需要在子线程中运行
    if ([[NSThread currentThread] isEqual:[NSThread mainThread]]) {
        NSLog(@"---⚠️当前处于主线程中运行[checkAckFromPeripheral:]，需要在子线程中才能运行此方法");
        return NO;
    } else {
        while (self.isReadData == NO) {
            [NSThread sleepForTimeInterval:0.1];
            if ([self.connectedPeripherals containsObject:peripheral] == NO) {
                return NO;
            }
            count++;
            if (count == 30) {
                NSLog(@"----⚠️判断是否3秒超时无收到数据");
                [self throwError:BleErrorReceiveDataTimeOut];
                return NO;
            }
        }
        return YES;
    }
    
}

/// 抛出错误
- (void)throwError:(BleError)bleError {
    if ([self.delegate respondsToSelector:@selector(BLEManagerError:)]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self.delegate BLEManagerError:bleError];
        });
    }
}

/// 扫描到蓝牙设备，添加到数组
- (void)addToDisPeripheral:(CBPeripheral *)peripheral advertisementData:(NSDictionary<NSString *,id> *)advertisementData {
    if ([self.scannedPeripherals containsObject:peripheral] == NO) {
        [self.scannedPeripherals addObject:peripheral];
        // 代理返回该外围设备
        if([self.delegate respondsToSelector:@selector(didDiscoverPeripheral:advertisementData:name:RSSI:)]){
            [self.delegate didDiscoverPeripheral:peripheral advertisementData:advertisementData name:peripheral.localName RSSI:peripheral.curRSSI];
        }
        NSLog(@"----搜索到BLE: %@, UUID:%@",peripheral.localName, peripheral.identifier.UUIDString);
    }
}

#pragma mark - setter/getter

- (CBCentralManager *)bmgr{
    if (!_bmgr) {
        _bmgr = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
    }
    return _bmgr;
}

- (NSMutableArray *)connectedPeripherals {
    if (!_connectedPeripherals) {
        _connectedPeripherals = [NSMutableArray new];
    }
    return _connectedPeripherals;
}

- (NSMutableArray *)scannedPeripherals {
    if (!_scannedPeripherals) {
        _scannedPeripherals = [NSMutableArray new];
    }
    return _scannedPeripherals;
}

- (NSMutableArray *)autoConnectUUIDs {
    if (!_autoConnectUUIDs) {
        _autoConnectUUIDs = [[NSMutableArray alloc] init];
    }
    return _autoConnectUUIDs;
}

- (NSMutableArray *)filterBLENameArray {
    if (!_filterBLENameArray) {
        _filterBLENameArray = [NSMutableArray array];
    }
    return _filterBLENameArray;
}


@end
