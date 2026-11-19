#pragma once

// The pixels of the alarm corner icons (docs/superpowers/specs/2026-11-19-alarms-design.md),
// drawn from the owner's preview of 2026-11-19: the same shapes on the active face (FacePage,
// row by row) and the always-on face (AlwaysOn, as a bitmap). Plain C++ for the host tests.
class AlarmIcons
{
public:
    enum Icon
    {
        SmallBell,  // inside the ring of the alarm's days
        Bell,
        Hourglass,
        IconCount
    };

    enum Pixel
    {
        Empty,
        Lit,   // the bell, or the hourglass's frame
        Sand   // the hourglass's sand, drawn only while it runs
    };

    static int width( Icon icon );
    static int height( Icon icon );
    static Pixel pixel( Icon icon, int x, int y );
};
