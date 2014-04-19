CrousConnectService
===================

Service for automatically connect to Ethernet Network at Crous Toulouse


INSTALL & USAGE

- In Configuration Properties ->
	- In C/C++ -> Preprocessor -> Preprocessor Definitions, add DEBUG_AS_APP to compile as a classic console program
	
- Login & Password:
	Put a file 'CrousConnectService_login.txt' (example provided in root directory) next to the service/exe with the following format (no spaces/tabs, only two lines):
	
login  
password  



- Install the service by executing the following command in the exe directory as admin (ex: admin cmd or powershell)
	- .\CrousConnectService.exe -install
	
- Remove the service by executing the following command in the exe directory as admin (ex: admin cmd or powershell)
	- .\CrousConnectService.exe -remove
	
- Launch Local Services (find it or Win+R -> services.msc)
	- Right click on 'Crous Connect Service'
	- Set Startup Type on 'Automatic' for automatic launch at startup
	
