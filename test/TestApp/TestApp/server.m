#include <sys/socket.h>
#include <netinet/in.h>
#import "server.h"

#define PORT 12345

static void trySetNonBlocking(int socket, const char *type) {
	int flags = fcntl(socket, F_GETFL, 0);
	int result = fcntl(socket, F_SETFL, flags | O_NONBLOCK);
	if (result == -1) {
		NSLog(@"[ERROR] Failed to set %s socket to be non-blocking (%d)", type, result);
	}
}

@implementation Connection

 - (id)initWithSocket:(int)remoteSock server:(Server*)_server {
	socket = remoteSock;
	readSource = nil;
	server = _server;
	
	setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, sizeof(int));
	trySetNonBlocking(socket, "connection\'s");

	readSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
										socket,
										0,
										server->dispatchQueue);

	dispatch_source_set_event_handler(readSource, ^{
		NSLog(@"[INFO] Remote peer disconnected");
		[self disconnect];
	});

	dispatch_resume(readSource);
	
	return self;
}

- (void)send:(NSString*)message {
	NSData* data = [message dataUsingEncoding:NSUTF8StringEncoding];
	dispatch_data_t buffer = dispatch_data_create(data.bytes,
												  data.length,
												  server->dispatchQueue,
												  DISPATCH_DATA_DESTRUCTOR_DEFAULT);

	dispatch_write(socket, buffer, server->dispatchQueue, ^(dispatch_data_t buffer, int error) {
		if (error != 0) {
			[self disconnect];
		}
	});
}

- (void)disconnect {
	if (socket != -1) {
		close(socket);
		socket = -1;
	}

	[server->connections removeObject:self];
	NSLog(@"[INFO] # connections: %lu", (unsigned long)[server->connections count]);
}

@end // end Connection

@implementation Server

- (instancetype)init {
	self = [super init];
	if (self) {
		serverSock = -1;
		connections = [[NSMutableArray alloc] init];
		messages = [[NSMutableArray alloc] init];
		dispatchSource = nil;
		dispatchQueue = nil;
		start = [NSDate date];
	}
	return self;
}

- (void)start {
	if (serverSock != -1) {
		return;
	}

	serverSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSock == -1) {
		NSLog(@"[ERROR] Failed to open TCP socket");
		return;
	}

	setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
	setsockopt(serverSock, SOL_SOCKET, SO_NOSIGPIPE, &(int){ 1 }, sizeof(int));

	trySetNonBlocking(serverSock, "server\'s listening");
	
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_len = sizeof(addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	
	if (bind(serverSock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
		NSLog(@"[ERROR] Failed to bind listening socket on port %d", PORT);
		close(serverSock);
		return;
	}
	
	if (listen(serverSock, 4) != 0) {
		close(serverSock);
		NSLog(@"[ERROR] Failed to start listening on port %d", PORT);
		return;
	}
	
	dispatchQueue = dispatch_queue_create("queue", DISPATCH_QUEUE_SERIAL);
	dispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, serverSock, 0, dispatchQueue);
	
	dispatch_source_set_event_handler(dispatchSource, ^{
		// accept the connection
		int remoteSock = accept(self->serverSock, NULL, NULL);
		if (remoteSock <= 0) {
			return;
		}
		
		Connection* conn = [[Connection alloc] initWithSocket:remoteSock server:self];
		if (!conn) {
			return;
		}
		
		[self->connections addObject:conn];
		NSLog(@"[INFO] # connections: %lu", (unsigned long)[self->connections count]);
		
		// send the header
		[conn send:@"Hello!"];
		
		for (NSString* message in self->messages) {
			[conn send:message];
		}
	});
	
	dispatch_resume(dispatchSource);
	
	[NSTimer scheduledTimerWithTimeInterval: 1.0
									 target: self
								   selector: @selector(onTick:)
								   userInfo: nil
									repeats: YES];
}

- (void)stop {
	if (serverSock != -1) {
		close(serverSock);
		serverSock = -1;
	}
	
	if (connections != nil) {
		for (id conn in connections) {
			[conn disconnect];
		}
	}
	
	if (dispatchSource != nil) {
		dispatch_source_cancel(dispatchSource);
		dispatchSource = nil;
	}
}

-(void)onTick:(NSTimer*)timer {
	NSDate* now = [NSDate date];
	NSTimeInterval delta = [now timeIntervalSinceDate:start];
	NSString* msg = [NSString stringWithFormat:@"Execution time = %f", delta];

	if ([connections count] > 0) {
		for (Connection* conn in connections) {
			[conn send:msg];
		}
	} else {
		[messages addObject:msg];
	}
}

@end // end Server
