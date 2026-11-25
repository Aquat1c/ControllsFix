# How ControllsFix works

## What the game reads

Eternal Fighter Zero reads each controller through DirectInput. For every
binding in `KEY.ini` it looks at one of two things: one of the first sixteen
buttons of the controller, or the left stick pushed past half of its travel.
It never looks at any other axis, at the D-Pad, or at buttons above the
sixteenth. EfzRevival adds the D-Pad as movement and nothing else.

On Xbox-type controllers the two triggers are not buttons. Windows reports
them to DirectInput as one shared axis: the left trigger pushes it one way and
the right trigger the other. The right stick is two more axes. None of this
reaches the game.

## What the mod changes

The mod changes the answer DirectInput gives when a program asks for the state
of a controller. After the real state has been read, the mod marks extra
buttons as pressed in it, according to the rules below, and hands the result
on. The game then finds a pressed button where it would have found nothing.

Both the game and EfzRevival ask through the same DirectInput call, so both
see the same result. A trigger can therefore also be used for an EfzRevival
hotkey.

Only buttons are ever added. A button the controller really reports as pressed
stays pressed.

## EfzRevival and the D-Pad

EfzRevival reads the D-Pad itself and uses it as movement for a player who has
nothing bound to the four directions. The mod never removes the D-Pad from the
controller state, so this keeps working with the mod loaded.

Giving the D-Pad directions button numbers in the mod's settings does not
switch that off. A D-Pad press is then the chosen button and, for a player
whose directions are unbound, movement at the same time. The setting is meant
for a player whose directions are bound to the stick or the keyboard and who
wants the D-Pad for something else.

## Keeping the 1P keys when joining

The game keeps each player's keys in two blocks, one for player 1 and one for
player 2. When you join an online match you are player 2, so EfzRevival swaps
the two blocks at the start: you then play with the keys you set up for 1P.

EfzRevival 1.02h and later also re-open the controllers during character
select when they see a controller they had not counted before, which is true
the first time they look. On the joining side they swap the two blocks again
right after that, without reading `KEY.ini` first. That undoes the swap from
the start of the match, and the joining player is left on the 2P keys. With
nothing bound for 2P the controller, or the keyboard, does nothing at all.

The mod repairs this. On every read of a device it remembers the two blocks.
It puts the remembered ones back only when all of this is true at the first
read after a controller was released:

- the blocks are exactly the remembered ones with their places exchanged,
- the two blocks are not the same as each other,
- the game is not in practice mode,
- the blocks now stand the way `KEY.ini` has them, player 1's keys in the
  first block and player 2's in the second, for one of its four presets.

The last point is what tells the mistake from a reload. A joining player's
blocks are meant to be the other way round from the file. After the mistaken
swap they match the file; after a reload of `KEY.ini` on the joining side they
are the file's keys exchanged, so they do not match, even when the file was
changed to have the two players' keys trade places. If `KEY.ini` cannot be
read, nothing is put back.

Practice mode is left out because there the blocks are exchanged on purpose,
by EfzRevival's own switch of sides and by training mods, and put back when
the match is left. That can fall together with the controllers being re-opened
and would look the same as the mistake. Online matches never run in practice
mode.

A swap with no controller released just before it, as at the start of a match
or when players are switched in practice mode, is never looked at.

The blocks are at a fixed place in the game's memory. The repair is only
active in the game exe that has them there, and it can be turned off with
`KeepBindingsOnJoin=0` under `[General]`.

## Why it works in online play

EfzRevival takes each player's input after the game has turned the controller
state into its own small set of bits: four directions and four buttons. Those
bits are what is sent to the other player and written into replays. The mod
acts before that step and only on the local machine, so nothing about the
game's simulation or about what is sent changes. The other player does not
need the mod.

## How the hook is installed

EfzModManager loads the mod's DLL into the game. The mod starts a short
thread that:

1. reads `ControllsFix.ini` from the mod's own folder,
2. loads the DirectInput of Windows from the system folder by its full path,
   because the `dinput.dll` in the game folder is the mod loader,
3. creates a DirectInput keyboard device, only to get at the table of
   functions that all DirectInput devices of a program share,
4. replaces the "get device state" and "release" entries of that table with
   its own functions,
5. releases the keyboard device again.

Keyboard and controller devices use the same table, so the replacement covers
the controllers the game opens later. The mod's function leaves every answer
alone that is not a controller state: it only acts when the caller asked for
the standard joystick structure and the read succeeded.

## Telling controllers apart

The first time a controller's state passes through, the mod notes two things
about it and keeps them:

- whether it is an Xbox-type controller. Windows marks those with `IG_` in the
  device path.
- which of the six extra axes exist (Z, X rotation, Y rotation, Z rotation and
  the two sliders) and what range of values each one has. Ranges differ: the game leaves them
  at the DirectInput default, while EfzConfig+ sets them to -1000 to 1000. The
  rules below work in percent of the range, so both give the same result.

What the mod knows about a controller is kept under the address of its
DirectInput device. EfzRevival closes and re-opens the controllers while the
game runs, for example when another one is plugged in or `KEY.ini` changes,
and a new device often lands on the address an old one just left, sometimes
the address of the other controller. So the mod also watches devices being
released: when the last reference to one goes away, everything noted about it
is thrown away, and the next device at that address is examined from scratch.

## The rules

A stick direction or a trigger counts as pressed when it is further from its
resting position than the threshold, which is a percentage of the way to the
end of its travel. The default is 50.

**Xbox layout** (Xbox-type controllers, or `Layout=Xbox`):

- The right stick is the X rotation axis (left and right) and the Y rotation
  axis (up and down).
- The triggers are read through XInput when possible, see the next section.
  Otherwise the shared Z axis is used: pushed one way it is the left trigger,
  the other way the right trigger. In that case holding both triggers cancels
  out and neither is seen.

**Classic layout** (all other controllers, or `Layout=Classic`):

- If the controller has a Z rotation axis, the right stick is the Z axis (left
  and right) and Z rotation (up and down). This is the PlayStation
  arrangement.
- If it has no Z rotation axis, the right stick is taken from the X and Y
  rotation axes.
- No triggers are mapped. On these controllers the triggers are ordinary
  buttons already. Their analog axes rest at one end instead of in the middle
  and are deliberately not read as a stick.

**Any layout:**

- Each trigger can be given an axis of its own: Z, X rotation, Y rotation, Z
  rotation or one of the two sliders. This is for controllers whose triggers
  are separate axes and not buttons, such as GameCube adapters and some
  generic pads. Such an axis rests at one end, so it is not measured from the
  middle: the trigger counts as pressed when the axis has moved further from
  its resting end than the threshold, as a percentage of its whole travel. The
  resting end is the low end unless `TriggerRest=High` says otherwise. The
  trigger presses the same button as the left or right trigger of the Xbox
  layout. An axis chosen this way is left out of the right stick, whatever the
  layout. Both are off by default.
- Each D-Pad direction can be given a button. A diagonal presses both of its
  directions. This is off by default, because EfzRevival uses the D-Pad for
  movement.
- Each of the buttons 17 to 32 can be given one of the sixteen buttons the
  game reads. This is off by default.

Each input has a button number from 1 to 16 in the ini file; 0 turns it off.
The defaults are 11 and 12 for the left and right trigger and 13, 14, 15 and
16 for right stick up, down, left and right.

## Reading both triggers at once

XInput reports the two triggers separately, but it numbers controllers one to
four on its own, and nothing says directly which of those is the DirectInput
device the game is reading. The mod finds out by comparing:

- If only one XInput controller is connected, it is that one.
- Otherwise it waits until something is pressed on the DirectInput device, a
  button or a trigger, and looks for the one XInput controller that shows the
  same ten buttons pressed and a trigger pulled the same way. If exactly one
  matches, that is the controller, and the mod remembers it.
- Until a match is found the shared axis is used.
- If the remembered XInput controller stops answering, the mod forgets it and
  starts over.

XInput is loaded from the Windows system folder. On a system without it the
shared axis is always used.

## Settings

`ControllsFix.ini` sits next to the DLL. A missing file, a missing key or a
value out of range means the default. The file is read once, when the game
starts.

The DLL exports a table of its settings named `EFZ_ConfigSchema`, in the
format EfzConfig+ reads, so the tool can show a Settings page for the mod
without the ini file having to exist first.

## Shared with EfzConfig+

`padmap.c` and `padmap.h` contain everything described under "Telling
controllers apart", "The rules" and "Reading both triggers at once". The mod
uses them inside the game; EfzConfig+ compiles the same files and applies them
to what it reads while you bind a key. That is why the tool shows button 11
when you pull the left trigger: it is what the game will see with the mod
installed. The tool reads the mod's ini file as well, so a changed number is
followed.

## Building

- Windows XP is the target: the v141_xp toolset, 32 bit, static C runtime, and
  only functions that exist on XP.
- The test, `test_padmap`, checks the rules against made-up controller states.
  It needs no controller.

## The logging build

The build also produces a second `ControllsFix.dll`, in the `log` folder of the
build output. It behaves the same as the normal one and additionally writes
`ControllsFix.log` into the mod's folder, starting a new file every time the
game starts. It is meant for finding out why a controller does not work on
someone's machine and is not the one to hand out for playing.

The log holds:

- the settings the mod read,
- every controller the mod meets, with its name, its device path and its axes,
- a controller's state whenever its buttons change, with the buttons the mod
  added,
- controllers being acquired, released and polled, with the module that asked,
  each time the result changes,
- what XInput reports,
- the contents of `KEY.ini`, again whenever the file is written,
- the keys the game currently holds in memory, whenever they change,
- the game's scene, the number of controllers it has open and whether its
  window is in front, whenever one of them changes,
- every time the mod puts the keys back as described under "Keeping the 1P
  keys when joining".
