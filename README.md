# 3proxy-eagle

`--help` output:

```
Accumulate ethical 3proxy statistics with web interface

U S A G E:
  -i  --instanse                  <3proxy>,<3proxy.cfg>
  -w  --working-directory         <data>
  -t  --service-title             <3proxy-eagle>
  -I  --ignored-destinations      <[0.0.0.0],0.0.0.0>
  -a  --bind-to-address           <127.0.0.1>
  -p  --bind-to-port              <8161>
  -l  --log-level                 <info> (off, error, warn, info, debug)
  -s  --top-lists-size            <5>
  -D  --reject-ip-from-statistics

N O T E S:
* Multi instanses supported. Just pass new one --instanse value!
* Main 3proxy cfg must contain log to stdout with strict format:
    log
    logformat " type=%N destination=%n to=%O from=%I"
  Also for normal logging by 3proxy main config should contain
    fakeresolve
  otherwise 3proxy may be very uninformative.
  Check "outproxy_config" folder as working example.
* 3proxy cfg can contain blocked domains in format:
    {{vk.com,mail.ru,google.com}}
  This domains will be resolved automatically and replaced by:
    deny * * original.domain
    deny * * 8.8.8.8 # resolved addresses
    deny * * 1.1.1.1
* If the working directory contains a information.html, the Information
  box will be added to the web page. The block is full html-formatted.
* The html folder can contain any files, they will be available
  for downloading through the web browser.
* html folder can contain styles.css file for overloading default styles.
  See page source via web browser to customize CSS classes.

GPLv3 (c) acetone, 2022
```