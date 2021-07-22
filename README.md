can-server

This an extensive hack of socketcand (and socketcandcl) in the repo linux-can/socketcand. 

The objective is a server similar to 'hub-server' (located in GliderWinchCommons/embed repo) that allows multiple tcp/ip connections to the server which will distribute incoming lines (ascii terminated with '\n') to the other connections, including the CAN interface.
