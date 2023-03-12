# arras4_log
## Arras logging libraries

The **arras4_log** library contains a basic implementation of logging to the console or syslog. It also contains **AutoLogger**, which can capture stdout/stderr from a computation process and log it.

**arras4_athena** adds support for the DWA **Athena** system, used to collect and index logs from many different machines. Athena works by capturing log lines sent to the syslog daemon in a specific format.

**arras4_crash** used the Google **breakpad** library to collect crash reports.

---

The current standard is to use the macros ARRAS_ERROR, ARRAS_WARN, ARRAS_INFO and ARRAS_DEBUG. These support operators that can supply an error ID for the log and also the current session ID (where appropriate). Error IDs are used to provide a keyword for indexing : typically we only use them with ARRAS_ERROR and ARRAS_WARN.

*Example:*
```
ARRAS_ERROR(log::Id("ExecFail") <<
		    log::Session(mAddress.session.toString()) <<
		    " : cannot find executable " << program << 
		    " on PATH for " << mName); 
```

