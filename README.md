
 IotaSlice
===========

Iota is my approach at a little powder based 3D printer.

IotaSlice is a crude pre-alpha version of software the slices
3DS formatted 3D models into bitmap slices. Every pixel then
corresponds to the ink fired from a nozzle of the 3D printer.

The Iota firmware includes a minimal interpreter that reads
files in a propritary language from SD card and creates the
appropriate signals for all steppers and the ink shield output
of the Iota machine, much like G-Code in the early days of
CNC machines.

Iota Slice uses OpenGL to create 2D surfaces out of 3D shapes.
Slices have a real height. They are not just 2D cuts through
the model, but real 3D slices of a give thinkness, projected
onto a 2D surface. This method may capture fine details that
may otherwise be lost.

Iota Slice has some crude ideas of a shell, creating different
ink densities around the model as opposed to the inside. There
are some first hooks to implement vertex-based color 
gradiations, but the 3DS interface is not yet implemented.
The next version will implement full color based on texture 
mapping.

Also, at some point, the entire code will have to be 
reorganized and cleaned and wrapped into a nice UI. Until
then, this code is purely educational for the brave.

 Matthias
