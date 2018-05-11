# Additive animation clips

Additive animation clips are a core tool within an animation runtime but sadly, how additive animations are blended and applied is not standard. As such, this library aims to support the most common additive modes. Contributions welcome!

The core logic for additive clips lives [here](../includes/acl/additive_utils.h).

## Relative additive clips

Clips in this space are applied on top of their base clip with a simple multiplication:

`result = additive * base`

This is the standard way to represent things in local space of something else. For example, every bone is in relative and local space of its parent bone. To convert a bone from local space to its parent space, we multiply them.

## Additive 0

*I couldn't come up with a better name for this, suggestions welcome!*

Clips in this space have their rotations multiplied with the base, the translations are added, and the scale is combined as follow:

`scale = additive_scale * base_scale`

Among others, this mode is used by [ozz-animation](http://guillaumeblanc.github.io/ozz-animation/).

## Additive 1

*I couldn't come up with a better name for this, suggestions welcome!*

Clips in this space have their rotations multiplied with the base, the translations are added, and the scale is combined as follow:

`scale = (1.0 + additive_scale) * base_scale`

This is the additive mode used by Unreal 4.
