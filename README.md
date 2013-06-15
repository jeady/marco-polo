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
and latency. Polo outputs the average latency (not yet implemented) and success
rate.

## Provided Programs
### Marco
Usage: ./marco [/dev/serial...]
Connects to the given serial port and tests the connectivity, assuming polo
is running on the other end.

```
jeady@olympus:/mnt/hgfs/shared/marco_polo$ ./marco /dev/pts/11
Transmitted 47 + 247 = 38.
Success! 100.00% success rate (1 / 1)
Transmitted 161 + 225 = 130.
Success! 100.00% success rate (2 / 2)
Transmitted 201 + 130 = 75.
Success! 100.00% success rate (3 / 3)
Transmitted 207 + 78 = 29.
Success! 100.00% success rate (4 / 4)
Transmitted 32 + 176 = 208.
```

### Polo PTY
Usage: ./polo_pty
Test program. When run, will open a pseudo-terminal which can be used to test
marco with another machine on the other end of the serial connection. Outputs
the terminal device to be used with marco on startup.

```
jeady@olympus:/mnt/hgfs/shared/marco_polo$ ./polo_pty 
PTY: /dev/pts/12
Received 114 + 112 = 226.
Received 161 + 187 = 92.
Received 194 + 229 = 167.
Received 196 + 202 = 142.
```

[1]: https://www.sparkfun.com/products/8946
[2]: https://www.sparkfun.com/products/10532
