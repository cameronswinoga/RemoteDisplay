# RemoteDisplay

RemoteDisplay is a RDP client library built upon [Qt](http://qt.digia.com/) and [FreeRDP](http://www.freerdp.com/).

The library consists of a single widget [RemoteDisplayWidget](src/remotedisplaywidget.h)
which renders the remote display inside it. The widgets sends any mouse movement
or keyboard button presses to the remote host as well as playback any audio the
remote host might playback.

## Usage example
```C++
#include <QApplication>
#include <QString>
#include <remotedisplaywidget.h>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

	// configuration options
	quint16 width  = 800;
	quint16 height = 600;
	QString host   = "1.2.3.4";
	quint16 port   = 3389;

	// create and show the RemoteDisplay widget
    RemoteDisplayWidget w;
    w.resize(width, height);
    w.setDesktopSize(width, height);
    w.connectToHost(host, port);
    w.show();

	// exit program if disconnected
    QObject::connect(&w, SIGNAL(disconnected()), &a, SLOT(quit()));

    return a.exec();
}
```
For full example, see the [example subdirectory](example).

## Building and installing
In order to use this library you need to build it first. Here's instructions
for Windows and Linux.

### Building in Linux
Prerequisites:
* Git
* CMake 2.8.11 or newer
* FreeRDP from https://github.com/pm-bernhard/FreeRDP
* Qt 5 or newer
* gcc 4.5 or newer (for some c++11 features)

#### Building FreeRDP
To build the FreeRDP, clone its repo first:
```
$ git clone https://github.com/pm-bernhard/FreeRDP
$ cd FreeRDP
$ To use the last tested version: git checkout tags/1.0.2
```
Next, cd to its directory and configure the project with cmake:
```
$ cmake -DCMAKE_BUILD_TYPE=Release -DWITH_SERVER:BOOL=OFF -DCHANNEL_AUDIN:BOOL=OFF -DCHANNEL_CLIPRDR:BOOL=OFF -DCHANNEL_DISP:BOOL=OFF -DCHANNEL_DRDYNVC:BOOL=OFF -DCHANNEL_DRIVE:BOOL=OFF -DCHANNEL_ECHO:BOOL=OFF -DCHANNEL_PRINTER:BOOL=OFF -DCHANNEL_RAIL:BOOL=OFF -DCHANNEL_RDPEI:BOOL=OFF -DCHANNEL_RDPGFX:BOOL=OFF -DCHANNEL_TSMF:BOOL=OFF .
```
There is a lot of features that RemoteDisplay doesn't use currently and so they
can be left off to produce slightly leaner binaries.
Finally build and install FreeRDP:
```
$ make
$ sudo make install
```

#### Building RemoteDisplay
First clone the repo:
```
$ git clone https://github.com/cameronswinoga/RemoteDisplay.git
$ git checkout updating
```
Next, cd to its directory and configure, build and install it:
NOTE: You may need to copy some file around/change where cmake is looking!
```
$ cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr .
$ make
$ sudo make install
```
Note, that if cmake complains of missing qtmultimedia component then sound playback
might not work, but it will not prevent compiling and using of the library.

Verify that the RemoteDisplay works by starting your RDP server and running
RemoteDisplay's example:
```
$ RemoteDisplayExample 1.2.3.4 3389 800 600
```
Run RemoteDisplayExample without parameters for explanation what the parameters do.

### Building in Windows
Prerequisites:
* Git
* CMake 2.8.11 or newer
* FreeRDP from https://github.com/FreeRDP/FreeRDP
* Qt 5 or newer
* Visual Studio 2013 (Cummunity Edition is enough)

For running commands use **Start > All Programs > Microsoft Visual Studio 2013 > Visual Studio Tools > Visual Studio Command Prompt (2013)**.

#### Building FreeRDP
To build the FreeRDP, clone its repo:
```
$ git clone https://github.com/FreeRDP/FreeRDP.git
$ cd FreeRDP
```
Next, cd to its directory and configure the project with cmake:
```
$ cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=C:/FreeRDP -G "NMake Makefiles" -DWITH_SERVER:BOOL=OFF -DWITH_OPENSSL:BOOL=OFF -DWITH_WINMM:BOOL=OFF -DCHANNEL_AUDIN:BOOL=OFF -DCHANNEL_CLIPRDR:BOOL=OFF -DCHANNEL_DISP:BOOL=OFF -DCHANNEL_DRDYNVC:BOOL=OFF -DCHANNEL_DRIVE:BOOL=OFF -DCHANNEL_ECHO:BOOL=OFF -DCHANNEL_PRINTER:BOOL=OFF -DCHANNEL_RAIL:BOOL=OFF -DCHANNEL_RDPEI:BOOL=OFF -DCHANNEL_RDPGFX:BOOL=OFF -DCHANNEL_TSMF:BOOL=OFF .
```
There is a lot of features that RemoteDisplay doesn't use currently and so they
can be left off to produce slightly leaner binaries.

Finally build and install FreeRDP:
```
$ nmake
$ nmake install
```

#### Building RemoteDisplay
First clone the repo:
```
$ git clone https://github.com/pm-bernhard/RemoteDisplay.git
$ git checkout updating
```
Next, cd to its directory and configure, build and install it:
```
$ cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=C:\RemoteDisplay -DWinPR_DIR=C:\FreeRDP\cmake\WinPR -DFreeRDP_DIR=C:\FreeRDP\cmake\FreeRDP .
$ nmake
$ nmake install
```
After installation verify that the RemoteDisplay works by starting your RDP
server and running RemoteDisplay's example:
```
$ cd C:\RemoteDisplay\bin
$ RemoteDisplayExample 1.2.3.4 3389 800 600
```
The example likely complains of missing dll files, in which case you can either
copy them to the bin directory or alternatively add their original locations
to PATH before executing the example:
```
set PATH=C:\<path to Qt>\bin;C:\FreeRDP\release;%PATH%
```
Run RemoteDisplayExample without parameters for explanation what the parameters do.

## License
Licensed under the ([MIT license](LICENSE)).
