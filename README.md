## Marco Polo
This is a tool that is intended to test the connectivity of an RF serial
connection, such as the one provided by [this transmitter][1] and
[this receiver][2].

## Packet Format
The sender, Marco, send packets of the following format.

| Index |  Data  |
|:-----:|--------|
|   0   |  'S'   |
|  1-2  | random |
|   3   |  'E'   |

The receiver, Polo, responds with the following packet format. All arithmatic
is unsigned.

| Index |  Data                   |
|:-----:|-------------------------|
|   0   |  's'                    |
|   1   | marco\[1\] + marco\[2\] |
|   2   |  'e'                    |

Polo verifies that the response arrives within 300ms (or whatever is
specified) then notes its correctness and latency. Polo outputs the average
latency and success rate.

## Provided Programs
### Marco
Usage:
```
./marco
  [-v]
  [-t timeout_ms=300]
  [-i idle_delay_ms=2]
  [-d transmit_delay_ms=1000]
  [-c count=100]
  -s /dev/serial-device
```

Connects to the given serial port and tests the connectivity, assuming polo
is running on the other end.

```
jeady@olympus:/mnt/hgfs/shared/marco_polo$ ./marco -s /dev/pts/13 -c 5
Device: /dev/pts/13
Count: 5
Debug: false
Delay: 1000ms
Idle: 2ms
Timeout: 300ms

0.00% success rate (0 / 1) 100.00% drop rate (1 / 1) 0.00% corrupt rate (0 / 1) avg. latency -nanms
50.00% success rate (1 / 2) 50.00% drop rate (1 / 2) 0.00% corrupt rate (0 / 2) avg. latency 90.53ms
33.33% success rate (1 / 3) 66.67% drop rate (2 / 3) 0.00% corrupt rate (0 / 3) avg. latency 90.53ms
25.00% success rate (1 / 4) 50.00% drop rate (2 / 4) 25.00% corrupt rate (1 / 4) avg. latency 90.53ms
20.00% success rate (1 / 5) 40.00% drop rate (2 / 5) 40.00% corrupt rate (2 / 5) avg. latency 90.53ms
```

### Polo PTY
Usage: `./polo_pty`

Used to test marco. When run, will open a pseudo-terminal which can be used to
test marco with another machine on the other end of the serial connection.
Outputs the terminal device to be used with marco on startup.

```
jeady@olympus:/mnt/hgfs/shared/marco_polo$ ./polo_pty 
PTY: /dev/pts/13
Received 173 + 127 = 44.
Not responding.
Received 82 + 8 = 90.
Delayed 90ms
Received 166 + 188 = 98.
Not responding.
Received 182 + 180 = 106.
Sending back corrupted data.
Received 103 + 245 = 92.
Sending back corrupted data.
```

[1]: https://www.sparkfun.com/products/8946
[2]: https://www.sparkfun.com/products/10532
