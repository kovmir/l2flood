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

```bash
l2flood 94:3a:2c:e1:2b:07 # Flood a given device using 30 threads.
l2flood -n 50 94:3a:2c:e1:2b:07 # Flood using 50 threads.
```

* The default delay between packets has been changed to `0`.
* The default data packet size has been increased to `600`.
* `l2ping` options work.

# DEPENDENCIES

* [Bluez][3]

# SUPPORTED OS

* Linux

# FAQ

**Q: Does it work in [termux][2]?**

A: No, [Bluez][3] libraries are not available in termux.

**Q: Does it work on Steam Deck?**

A: Yes.

[1]: https://linux.die.net/man/1/l2ping
[2]: https://github.com/termux/termux-app
[3]: https://wiki.archlinux.org/title/Bluetooth
