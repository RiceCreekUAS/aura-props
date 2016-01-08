# aura-props
A pure python "Property Tree" system for: convenient sharing of shared data between modules, organizing data, sharing state between python and C modules.

## Background

Every non-trivial program needs a way to share data between modules.
Traditional data encapsulation involves hiding and protecting a
module's data so that other modules cannot directly touch it.  Instead
any interaction with the module's data is through the module's
carefully crafted API.  This is called "data encapsulation" and it is
at the foundation of object oriented programming.

Constructing an application's modules and data in a carefully packaged
way is a great idea.  It is a very robust way to structure your
applications and avoid a large class of bugs that arise out of
careless data sharing.  However, every ideology has to eventually
survive on it's own in the real world.  There is a reason C++ classes
have static (shared) members and friend functions.  There is a reason
C++ supports global variable space.  Real world programming challenges
are often messier than the book examples.

There are times when it makes the most sense to share data globally
within your application. It just does.  (Sometimes it still makes
sense to use a goto.)

Aura-props provides a way to create shared data space within your
application in a friendly and well structured way.  With aura-props as
the backbone of your application, you receive many additional nice
services and structures.

## The "Property Tree"

The aura-props module enables an application to easily build a tree of
important data to be shared throughout the program.  There is a single
shared 'root' node that is automatically created when the props module
is imported.  From there an application can start filling in the tree
very quickly.  For example, consider a reader/writer example of a
simple autopilot that reads an external gps and uses that data to
navigate.

A hierarchical tree structure is easy to understand, easy to organize,
easy to use, and keeps like data near each other.

### The writer module

The gps driver module could include the following (python) code:

```
from props import getNode, root
gps = getNode("/sensors/gps", create=True)
(lat, lon, alt, speed, ground_track) = read_gps()
gps.lat = lat
gps.lon = lon
gps.alt = alt
gps.speed = speed
gps.ground_track = ground_track
```

With this simple bit of code we have constructed our property tree,
read the gps, and shared the values with the rest of our application.
Done!

The getNode() function will find the specified path in the property
tree and return that node to you.  If you specify create=True, then
getNode() will automatically create the node (and all the parents and
grandparents of that node) if they don't already exist.  So in one
line of code we have constructed the portion of the property tree that
this module needs.

A pyPropertyNode is really just an open ended python class, so we can
then assign values to any attributes we wish to create.

### The reader module

The navigation module needs the gps information from the property tree
and do something with it.  The code starts out very similar, but watch
what we can do:

```
from props import getNode, root
gps = getNode("/sensors/gps", create=True)
waypoint = getNode("/navigation/route/target", create=True)
ap = getNode("/autopilot/settings", create=True)
heading = great_circle_route([gps.lat, gps.lon], [waypoint.lat, waypoint.lon])
ap.target_heading = heading
```

Did you see what happened there?  We first grabbed the shared gps
node, but we also grabbed the shared target waypoint node, and we
grabbed the autopilot settings node.  We quickly computed the heading
from our current location to our target waypoint and we wrote that
back into the autopilot configuration node.

This approach to sharing data between program modules is a bit unique.
But consider the alternatives: many applications grow their
inter-module communication ad-hoc as the code evolves and some of the
interfaces can become inconsistent or awkward as real world data gets
incrementally shoved into existing C++ class api's.  The result of the
ideological approach is often messy and clunky.

The property system provides an alternative for intra-application data
sharing that is simple, easy to understand, and just works.  It is a
different philosophy of programming from what many people are used to,
but the benefits and convenience of the property system quickly
becomes a way of life.

### Initialization order.

Please notice that both the reader and writer modules in the above
example call getNode() with the create flag set to true.  This allows
initialization order independence.  No matter which module is called
first, the tree is created properly.

### Direct access to properties

The property tree is constructed out of a thin python shell class.
Once the appropriate portions of the property tree are created and
populated, python code can directly reference nodes and values.  For
example, the following reference should work if the gps sensor tree
has been created and populated:

```
lat = props.sensors.gps.lat
```

## Sharing data between mixed C++ and Python applications

A C++ interface to the python property tree is being developed in
parallel.  For now, know that it exists and brings to C++ most of the
benefits of the property tree.  (And also enables data sharing between
applications that are a mix of C++ and Python.)

## Script features for C++

For the C++ developer: incorporating the Property Tree into your
application brings several conveniences of scripting languages to your
application.  One big convenience is automatic type conversion.  For
example, an application can write a string value into a field of the
property tree, but read it back out as a double.  Watch carefully:

```
#include "pyprops.hxx"
int main(int argc, char **argv) {
    // cleanup the python interpreter after all the main() and global
    // destructors are called
    atexit(pyPropsCleanup);
    
    pyPropsInit(argc, argv);

    pyPropertyNode gps_node = pyGetNode("/sensors/gps");

    gps_node.setString("lat", "-45.235");
    double lat = gps_node.getDouble("lat");
```

Did you see how the value of "lat" is written as a string constant,
but can be read back out as a double?  Often it is easy to keep your
types consistent, but it's nice to just let the back end system convert
types for you as needed.
 
### Performance considerations

Convenience comes at a cost.  The property tree has been designed in a
way to leverage existing native python structures so it is relatively
thin and fast, but within python scripts, saving a variable as a class
member does have more overhead that a standalone variable.  Within
C++, accessing property nodes and values requires a call layer into
python structures.  Thus reading and writing properties does involve
some additional overhead compared to using native variables.

The best recommendation is to place all calls to `getNode()` within a
modules initialization routine, cache the pointer that is returned,
and then use this pointer exclusively in the module's update routines.

This way the expensive getNode() function is only called during
initialization, and the faster class.field notation (Python) or get*()
set*() routines (C++) are called during run-time.

## Easy I/O for reading and writing configuration files

The hierarchical structure of the property tree maps nicely to xml and
json. (Todo: expand this section.)

## A note on threaded applications

The Property Tree system is *not* thread safe.  I am pondering some
ideas to make a thread safe version of the property tree, but this
will add restrictions to the api and overhead for resource locking.
Hopefully I will add more on this later.

For now, if you include threads in your application, know that either
the property tree should be confined exclusively to one thread, or you
will need to take extra precautions within your own application to
ensure two threads do not try to read or write the property tree
simultaneously.  Doing so could lead to seaming random and difficult
to debug program crashes.