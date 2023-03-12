# standalone computation host

This is a minimal standalone implementation of a computation host. You can use it to implement simple test programs
that load and run computations.

Begin by creating a **StandaloneEnvironment** instance. The simpler option is to pass it the name of the computation dso that
you want to load. This will then be loaded using the normal dlopen() mechanism, used elsewhere by Arras.

Alternatively, you can write a factory function that builds an instance of your computation given an environment, In many
cases the following will be fine:

> Computation* factory(ComputationEnvironment* env) { return new MyComputation(env); }

You also need a message handler function with the signature

> void handleMessage(const Message& message)

This is written like a computation or client message handler, i.e.

```
if (message.classId() == MyMessageClass::ID) {
   ...
}
```

Once you have created the environment, construct a configuration object and initialize it. You should normally set at
least ConfigNames::maxMemoryMB and ConfigNames::maxThreads, as these are usually set automatically by Arras. e.g.

```
config[ConfigNames::maxMemoryMB] = MAX_MEMORY_MB;
config[ConfigNames::maxThreads] = MAX_THREADS;
config["fps"] = FPS;
if (env.initializeComputation(config) == Result::Invalid) {
      std::cerr << "ERROR: Initialization failed" << std::endl;
      ...
}
```

To send messages to the computation, construct the content objects directly and call sendMessage (as you would in a client)

```
MessageContentConstPtr cp(new MyContentClass(...));
env.sendMessage(cp);
```

Some computations will require onIdle calls to function correctly : you can call this in a loop, for example:

```
while (true) {
     env.performIdle();
     std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
```

During the calls to `sendMessage` and `performIdle`, the computation is likely to call your message handler function to send
back results. There is no queueing or threading in StandaloneEnvironment.
