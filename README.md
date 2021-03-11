# RayTracer
A raytracer written in C++ for Windows, Linux & WebAssembly.

**Try it: <https://raytracer.clemy.org/>** (use Chrome or Firefox)

## Features
* Read scene files described here: [input file specification](http://vda.univie.ac.at/Teaching/Graphics/20w/Labs/Lab2/lab2_file_specification.html)
* extended scene file with more features (see Extensions chapter below)
* Spheres & [triangle meshes](https://en.wikipedia.org/wiki/Wavefront_.obj_file)
* Julia sets (quaternion set fractals)
* Animations
* Depth of field
* Motion blur
* Fresnel reflection, refraction & extinction
* Light Dispersion
* Antialising using supersampling
* Anti-aliased textures
* Caustics
* Multithreading
* Only 1 library dependency (libz)
* (Animated) PNG output (without needing libpng)
* Compiles to WebAssembly

# Local Build

## Requirements
* libz compression library + dev headers
    * Ubuntu: `sudo apt install zlib1g-dev`
    * Windows: automatic download triggered by `package.config`
* C++17 compiler
    * tested: gcc 7.5.0
    * tested: MS Visual Studio 2019
* for WebAssembly(WASM) see end of readme

## Build (Linux)
```bash
make
```

## Run (Linux)
```bash
# append the scene description filename to the command, like:
./raytracer examples/example1.xml
```

## Build (Windows)
* Open `raytracer1.sln` in MS Visual Studio 2019 (Community Edition)
* Switch to `x64` and `Release` in the tool bar
* Build Solution
* find executable in `x64\Release\raytracer1.exe`

## Running all examples
Please use the script files `run_examples*` for running multiple examples.

* The directory `examples` contains examples for basic features as described [here](http://vda.univie.ac.at/Teaching/Graphics/20w/Labs/Lab2/lab2a.html).
* The directory `examples2` contains examples for extended features (see below).
* The directory `examples3` contains examples with long render times (1 - 3 min).

Be aware: File names in the scene xml file are relative to the scene xml file (mesh `.obj` and texture `.png`). But the output file is relative to the current directory.

# Copyright
* Raytracer: Copyright 2020-2021 by Bernhard C. Schrenk <https://www.clemy.org/>
  * Licensed under GPLv3
  * Ideas and code fragments from external sources marked by comments in the source files
* Examples: Taken from <http://vda.univie.ac.at/Teaching/Graphics/20w/Labs/Lab2/lab2a.html>

# Extensions
Tags for following extensions were added to the XML scene file on top of the official specification:
* Alpha Channel
* Animations
* Motion Blur
* Julia Sets
* Multi Threading
* Supersampling
* Depth of Field
* Fresnel Refraction with extinction and dispersion
* Caustics

## Alpha Channel
### Example: examples2/1_transparent.xml
Colors can have an alpha channel. This is useful for the `background_color` to
generate PNGs with **transparent** backgrounds.
It gets specified with the optional attribute `a`. The default value is '+1.0'. Example for transparent background: `<background_color r="0.0" g="0.0" b="0.0" a="0.0"/>`  
On lights and materials the alpha channel gets ignored.

## Animations
### Example: examples2/2_animation.xml
The raytracer can generate animations by animating any floating point value in the scene file. To enable this mode a new tag on the `<scene>` must be added: `<animation length="3.0" fps="25.0"/>`. This activates rendering an animation with `length` seconds and `fps` frames per second. Be aware that this can take a long time. The example would render 75 images.

### Animating a value
Every floating point value in the scene XML can be animated, except (in XPath notation):
* scene\\@threads
* scene\\@time
* scene\\animation\\@length
* scene\\caustic\\@texture_size
* camera\\resolution\\@*
* camera\\supersampling\\@subpixels_peraxis

There can be values assigned to keyframes. Keyframes get assigned to time points. The time is running from 0.0 to 1.0. The values in between keyframes get interpolated (using a selectable ease function). The format for a value with keyframe is: `value(easeFunction,time)`. Multiple values get separated by semicolon(`;`). There must not be spaces in the string.

Following command animates from `2.0` to `3.0` over the whole time (`0.0` to `1.0`) and uses the bidirection cubic ease function(`b`): `"2.0(b,0.0);3.0(b,1.0)"`

For the first and last keyframe the time can be ommitted. And for the first keyframe the ease function is irrelevant. Further the values can be shorten. So the whole command can be shorten to: `"2;3(b)"`

Following command animates from `2.0` to `3.0` and back to `2.0`. The last used ease function gets remembered, so it is not necessary to specify it again on the third keyframe: `"2;3(b,0.5);2"`

If you want to use the default ease function (linear) it can be ommitted (and the keyframe time can still be set): `"2;3(0.5);2"`

Following ease functions are available:
| type | ease function            |
|-----:|--------------------------|
|  `l` | linear                   |
|  `i` | cubic ease in            |
|  `o` | cubic ease out           |
|  `b` | cubic ease bidirectional |

### Still Images
It is possible to generate one image out of an animation by adding the tag `<still time="0.1"/>` as child of `<scene>`. This is interesting to test a single time point or to render a still image with motion blur (see below).

### Animations Output File Format: APNG
The output file format for animations is APNG which
is backward compatible with PNG for viewing still images.  
For the animation effect please show the .png file in an
**APNG** compatible **viewer** like **Google Chrome**.

## Motion Blur
### Example: examples2/3_motionblur.xml
Motion blur can be enabled for animations with the new tag `<motionblur subframes="10"/>` as child of the `<scene>` tag. The attribute `subframes` defines the number of intermediate pictures rendered for each frame. This attribute itself can be animated to adapt to phases with different motion content.

Motion blur can only be used with animations, but it is possible to render a single image with motion blur using the `<still>` tag (see above or the example).

## Julia Sets
### Example: examples2/4_julia.xml
There is support for julia sets with the generator function `f(x) = x^2 + c` on the set of quaternions. To render a julia set add it to the scene like any other surface using the new `<julia>` tag and add child tags as for any other surface. Only `<material_solid>` is supported. Texturing is not available for julia sets due to the infinite surface of a julia set and my limited imagination how I could map the texture on it. An example julia set tag looks like: `<julia scale="1" cr="-0.291" ca="-0.399" cb="0.339" cc="0.437" cutplane="0">`.

The attributes `cr`, `ca`, `cb`, `cc` are the 4 real parameters of the quaternion `c`. Julia sets only exists for `c` out of the Mandelbrot set. This means, `c` must be choosen (or googled) wisely to see something. The attribute `cutplane` defines the position of the hyperplane on the 4th dimension axis used to cut a slice out of the julia set to project it into the 3 dimensional space. The hyperplane is always parallel to the hyperplane defined by the first 3 axis. `cutplane` is a nice parameter for animations, try `cutplane="-1;1"`.

The attribute `scale` allows scaling, but this can also be done with transformations.

Further examples:
* examples2/5_julia_animation.xml
* examples3/101_julia_shiny.xml
* examples3/104_julia_animation.xml

## Multi Threading
The raytracer uses multiple threads. This can be limited with the new optional `<scene>` attribute `threads`. To limit to 1 thread the tag `<scene output_file="out.png" threads="1">` can be used.

## Supersampling
### Example: examples2/6_supersampling.xml
There is support for supersampling using the new tag `<supersampling subpixels_peraxis="3"/>` as subnode of the `<camera>` tag. The attribute `subpixels_peraxis` gives the number of subpixels generated per pixel per axis. This means the value `3` will use `3 * 3 = 9` subpixels for every pixel.

## Depth of Field
### Example: examples2/7_dof.xml
Depth of Field gets activated with the tag `<dof>` as subnode of the `<camera>` tag. Attributes `x`, `y` and `z` define the focus point. The attribute `lenssize` defines the size(aperture) of the lens. An example tag looks like: `<dof x="0.0" y="0.0" z="-5" lenssize="0.15"/>`.

## Fresnel Refraction with extinction and dispersion
### Example: examples2/8_fresnel.xml
The Fresnel caculation supports the use of an extinction coefficient (internally modelled with a complex refractive index). This can be used for modelling conductors like metals. The extinction coefficient can be set with the new attribute `ec` on the `<refraction>` tag. An example for metal silver is `<refraction iof="0.15016" ec="3.4727"/>`.

Further there is the possibility to specify a dispersion coefficient `disp` to configure a wavelength dependent refraction. The coefficient must specify a factor between wavelength in the visible range from -1.0(red) to 1.0(purple) and an offset to the refraction coefficient.

Further examples:
* examples3/102_fresnel_animation.xml

## Caustics (PhotonMapping)
### Example: example2/9_caustic.xml
There is rudimentary support for generating caustic effects. It gets enabled with the new tag `<caustic steps="2400" texture_size="300" factor="0.0225"/>` as subnode of `<scene>`. The `steps` attribute defines the number of steps the circle around a light source gets divided into to cast sample rays. As sample rays are casted on a globe around the light source, the number of rays increases with `steps` by the power of 2. The attribute `texture_size` defines the horizontal and vertical texture size used to store the caustic pattern for each object. The attribute `factor` determines the factor the caustic effects the final radiance. At the moment this needs to be adapted together with the other attributes to get a good effect. Factor has to be changed by a factor which is the power of 2 of the factor changing one of the other attributes.

Further examples:
* example2/10_caustic_texture.xml
* example3/103_caustic_animation.xml

# WebAssembly (RayTracer in the Browser)
The directory `wasm/out` contains a pre-built WebAssembly version of the raytracer. Just serve that directory via a webserver and open `http://localhost:port/raytracer.html` in **Chrome**. (Safari is missing a necessary feature and Firefox requires special CORS HTTP headers.)

The binary can be rebuilt from the source code with the script `wasm/compile.sh`, but it requires a recent emscripten toolchain.
