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

Polo verifies that the response arrives within 300ms, and notes its correctness
and latency. Polo outputs the average latency and success rate.

## Provided Programs
### Marco
Usage: `./marco [-v] /dev/serial-device`

Connects to the given serial port and tests the connectivity, assuming polo
is running on the other end.

```
jeady@olympus:/mnt/hgfs/shared/marco_polo$ ./marco /dev/pts/12
Incorrect sum. 0.00% success rate (0 / 1), avg. latency -nanms
Incorrect sum. 0.00% success rate (0 / 2), avg. latency -nanms
Success! 33.33% success rate (1 / 3), avg. latency 0.36ms
Timeout, resending. 25.00% success rate (1 / 4), avg. latency 0.36ms
Success! 40.00% success rate (2 / 5), avg. latency 0.29ms
Success! 50.00% success rate (3 / 6), avg. latency 0.25ms
```

### Polo PTY
Usage: `./polo_pty`

Used to test marco. When run, will open a pseudo-terminal which can be used to test
marco with another machine on the other end of the serial connection. Outputs
the terminal device to be used with marco on startup.

```
jeady@olympus:/mnt/hgfs/shared/marco_polo$ ./polo_pty 
PTY: /dev/pts/12
Received 221 + 51 = 16.
Sending back corrupted data.
Delay 1698ms
Received 51 + 1 = 52.
Sending back corrupted data.
Delay 2462ms
Received 202 + 232 = 178.
Delay 2470ms
Received 147 + 46 = 193.
Not responding.
Delay 1176ms
Received 86 + 66 = 152.
Delay 3136ms
Received 142 + 163 = 49.
Delay 3546ms
```

[1]: https://www.sparkfun.com/products/8946
[2]: https://www.sparkfun.com/products/10532
