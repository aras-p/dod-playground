# dod-playground

Sample OOP/ECS/DOD project (C++) for an internal Unity lecture in 2018, aimed at junior/future engineers.

This is a super-simple C++ "game" _(just a bunch of sprites moving around with very small amount of logic really)_ I made to
show how one might go from a very traditional OOP style GameObject/Component based code to an ECS _(Entity Component System)_ /
DOD _(Data Oriented Design)_ based code. In the process making it run 10x faster, initialize 5x faster, and saving 100MB of memory
in the process.

Talk [**slides are here**](http://aras-p.info/texts/files/2018Academy%20-%20ECS-DoD.pdf) _(10MB pdf)_.

The app playground should work on Windows (uses D3D11, VS2017 project in `projects/vs2017/dod-playground.sln`) and
macOS (uses Metal, Xcode 9 project in `projects/xcode/dod-playground.xcodeproj`).

I used some excellent other libraries/resources to make life easier for me here:

* [Sokol](https://github.com/floooh/sokol) libraries for application setup, rendering and time functions. zlib/libpng license.
* [stb](https://github.com/nothings/stb) library for image loading. mit/unlicense license.
* Sprites used by "the game" I took from Dan Cook's [Space Cute prototyping challenge](http://www.lostgarden.com/2007/03/spacecute-prototyping-challenge.html).
