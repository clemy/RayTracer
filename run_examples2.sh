make
mkdir -p out2
cd out2
../raytracer ../examples2/1_transparent.xml
../raytracer ../examples2/2_animation.xml
../raytracer ../examples2/3_motionblur.xml
../raytracer ../examples2/4_julia.xml
../raytracer ../examples2/5_julia_animation.xml
../raytracer ../examples2/6_supersampling.xml
../raytracer ../examples2/7_dof.xml
../raytracer ../examples2/8_fresnel.xml
../raytracer ../examples2/9_caustic.xml
../raytracer ../examples2/10_caustic_texture.xml

cd ..
