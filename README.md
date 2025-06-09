# floppi
Working to build an open source autonomously controlled mini quadcopter (not quite micro yet) 
> [base system for flight controller software](https://github.com/nickrehm/dRehmFlight)

### Setup Dev Environment

[Install platformio](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html)

```
curl -fsSL -o get-platformio.py https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py; python3 get-platformio.py
```

add to path temporarily

```
export PATH="$PATH:/home/codespace/.platformio/penv/bin/"
```

add to path in bashrc

```
echo 'export PATH="$PATH:/home/codespace/.platformio/penv/bin/"' >> ~/.bashrc; source ~/.bashrc
```
