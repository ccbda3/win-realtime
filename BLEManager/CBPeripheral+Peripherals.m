//
//  CBPeripheral+Peripherals.m
//  BLEDemo
//
//  Created by hyh on 25/1/2021.
//

#import "CBPeripheral+Peripherals.h"
#import <objc/runtime.h>

@implementation CBPeripheral (Peripherals)

static char keyWritable;
//static char keyRename;
static char keyConnected;
static char keyCurRSSI;
static char keyReadCharacteristic;
static char keyWriteCharacteristic;
static char keyLocalName;

// writable的setter方法
- (void)setWritable:(NSString *)writable {
    
    [self willChangeValueForKey:@"keyWritable"];
    objc_setAssociatedObject(self,
                             &keyWritable,
                             writable,
                             OBJC_ASSOCIATION_COPY_NONATOMIC);
}

// writable的getter方法
- (NSString *)writable {
    
    return objc_getAssociatedObject(self, &keyWritable);
}


// connected的setter方法
- (void)setConnected:(NSString *)connected {
    
    [self willChangeValueForKey:@"keyConnected"];
    objc_setAssociatedObject(self,
                             &keyConnected,
                             connected,
                             OBJC_ASSOCIATION_COPY_NONATOMIC);
}

// connected的getter方法
- (NSString *)connected {
    
    return objc_getAssociatedObject(self, &keyConnected);
}


// rename的setter方法
- (void)setRename:(NSString *)rename {
    
    [[NSUserDefaults standardUserDefaults] setObject:rename forKey:[NSString stringWithFormat:@"%@",self.identifier.UUIDString]];
    
//    [self willChangeValueForKey:@"keyRename"];
//    objc_setAssociatedObject(self,
//                             &keyRename,
//                             rename,
//                             OBJC_ASSOCIATION_COPY_NONATOMIC);
}

// rename的getter方法
- (NSString *)rename {
    
    NSString *name = [[NSUserDefaults standardUserDefaults] objectForKey:[NSString stringWithFormat:@"%@",self.identifier.UUIDString]];

//    NSString *name = objc_getAssociatedObject(self, &keyRename);
    
    if (name.length >= 1) {
        return name;
    }
    
    return self.name;
}


// curRSSI的setter方法
- (void)setCurRSSI:(NSNumber *)curRSSI {
    
    [self willChangeValueForKey:@"keyCurRSSI"];
    objc_setAssociatedObject(self,
                             &keyCurRSSI,
                             curRSSI,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

// curRSSI的getter方法
- (NSNumber *)curRSSI {
    
    return objc_getAssociatedObject(self, &keyCurRSSI);
}


// readCharacteristic的setter方法
- (void)setReadCharacteristic:(CBCharacteristic *)readCharacteristic {
    
    [self willChangeValueForKey:@"keyReadCharacteristic"];
    objc_setAssociatedObject(self,
                             &keyReadCharacteristic,
                             readCharacteristic,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

// readCharacteristic的getter方法
- (CBCharacteristic *)readCharacteristic {
    
    return objc_getAssociatedObject(self, &keyReadCharacteristic);
}


// writeCharacteristic的setter方法
- (void)setWriteCharacteristic:(CBCharacteristic *)writeCharacteristic {
    
    [self willChangeValueForKey:@"keyWriteCharacteristic"];
    objc_setAssociatedObject(self,
                             &keyWriteCharacteristic,
                             writeCharacteristic,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

// readCharacteristic的getter方法
- (CBCharacteristic *)writeCharacteristic {
    
    return objc_getAssociatedObject(self, &keyWriteCharacteristic);
}

- (void)setLocalName:(NSString *)localName {
    [self willChangeValueForKey:@"keyLocalName"];
    objc_setAssociatedObject(self,
                             &keyLocalName,
                             localName,
                             OBJC_ASSOCIATION_COPY_NONATOMIC);
}

- (NSString *)localName {
    return objc_getAssociatedObject(self, &keyLocalName);
}

@end
