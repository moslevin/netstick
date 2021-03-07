project(netstick)

set(SERVER_SRC 
	jsproxy.c
	server.c
	slip.c
	joystick.c
	tlvc.c
)

set(CLIENT_SRC
	jsclient.c
	slip.c
	joystick.c
	tlvc.c
)

add_executable(netstickd ${SERVER_SRC})
add_executable(netstick ${CLIENT_SRC})
