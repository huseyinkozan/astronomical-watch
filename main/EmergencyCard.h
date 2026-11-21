#pragma once

// The tools page's emergency card (ToolParts): a few lines for a helper, set in the setup portal
// and kept in NVS. The texts are UTF-8 in the watch font's letters (ASCII and the Turkish ones;
// the portal leaves the others out), at most one table cell long (TableText::CellSize).
struct EmergencyCard
{
    enum Field
    {
        Name,
        BloodType,
        Allergies,
        Medication,
        ContactName,
        ContactPhone,
        Note,
        FieldCount
    };
    static const int FieldSize = 32;  // bytes with the final 0

    char fields[FieldCount][FieldSize];

    // True when every field is empty: the part then stays hidden.
    bool empty() const;

    // `text` into the field, cut at a whole UTF-8 letter; quotes, backslashes and control
    // characters become spaces (the portal sends the card back as JSON).
    void set( Field field, const char *text );

    // Reads the saved card; an empty one when there is none.
    void load();
    void save() const;
};
