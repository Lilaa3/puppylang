# puplang (joke between a friend that i had to make)

![Bible but about puppygirls and catgirls in puplang](images/example.webp)
So you might think, Lila, why would I need to write out my messages like a dog with random howls inbetween the barks. Well.. Uhm.. Mm...

You might also be asking, how does it work! Well imagine like a discord puppygirl's brain got put into some bad cpp trying to be all modern and shit. It's like that. But very convuluted UTF-8. I can explain it like a real human being too if you want I suppose.,,

<sup>why would you ask me [to do that](MECHANISMS.md).,.</sup>

## Building

### C++

Dependencies: [CMake](https://cmake.org/) >= 3.10 and a C++26 compiler (clang 17+ or gcc 14+).

The C++ implementation is split into a header-only core library (`lib/`)
and a command-line frontend (`cli/`). The default sound table
(`lib/default_settings.txt`) is embedded into the library at compile time
via `#embed`, so no runtime settings file is needed unless you pass one.

```sh
cmake -S . -B build
cmake --build build
```

### Web

Dependencies: [Emscripten](https://emscripten.org/) (latest), [Node.js](https://nodejs.org/) (latest), and npm (for `marked`, installed automatically via `npm install`).

```sh
npm install --prefix web
bash web/build.sh
```

## Contributing

#### why.

<sup>Clang format please and don't be a dick and don't change the standard if you aren't me.. That's all.</sup>

## Use cases

Uhm... Encrypt your messages like a terminally online trans woman. Or double encrypt them! Now even the CIA won't be able to know about the disgusting things someone would use this for. Probably.
![It says lesbian if you are wondering](images/double_crypt.webp)

## It can do your homework

Thats right the opposite of a dumb dog is one that speaks AND does your homework.

## Wanna disect this dog's brain?

You can dump the entire sound table straight from the CLI:

```sh
./puplang sounds -o sound_table.txt
```

It includes every word alongside its equivalent value and ASCII C representation (for example `\n` or `\u{0}` or just `A`).
You can also download it from the releases/artifacts.

## Development status

I'd really like to add stuff like.
- Binary mode: sounds funny
- ~~More optimized bark mode, similar to purrcrypt?~~
- Compression?? Not out of the question?
- Versioning + a magic number.. Maybe.