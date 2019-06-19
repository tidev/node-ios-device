#ifndef server_h
#define server_h

#import <Foundation/Foundation.h>

@class Server;

@interface Connection : NSObject {
	int socket;
	dispatch_source_t readSource;
	Server* server;
}

- (id)initWithSocket:(int)remoteSock server:(Server*)server;
- (void)send:(NSString*)message;

@end

@interface Server : NSObject {
	int serverSock;
	NSMutableArray<NSString*>* messages;
	dispatch_source_t dispatchSource;
	NSDate* start;

	@public
	NSMutableArray<Connection*>* connections;
	dispatch_queue_t dispatchQueue;
}

- (void)start;
- (void)stop;
- (void)onTick:(NSTimer*)timer;

@end

#endif
