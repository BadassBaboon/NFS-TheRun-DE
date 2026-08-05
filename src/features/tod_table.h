#ifndef TOD_TABLE_H
#define TOD_TABLE_H

#include <cstdint>
#include <cstddef>

// Which time-of-day presets each level may legally use.
//
// GENERATED from _mRally2's TOD Randomizer cheat table; do not hand-edit. That
// table is a Lua if/elseif chain keyed on a level GUID, each arm picking from
// that level's own set of presets. The sets differ because not every level
// implements every time of day -- the source notes "NIGHT TOD IS NOT IMPLEMENTED"
// against Get Outta San Francisco, for one -- so choosing a preset a level cannot
// render is how you get a broken or black scene. That is why this is a per-level
// table and not a random number in a range.
//
// Every arm was checked as it was converted: the number of presets in each set
// matches that arm's own math.random(1,N) bound, and no GUID appears twice.
//
// Car Crusher is deliberately absent. The source reaches it through a different
// pointer chain and writes a second value for New Jersey Junkyard; that is not
// replicated here, so this does not claim to handle it.

struct TodEntry {
    uint32_t guid;
    uint8_t  count;
    uint8_t  presets[5];
};

static const TodEntry kTodTable[] = {
    { 3969471516u, 2, { 0, 3, 0, 0, 0 } },  // What a Rush
    { 3905447374u, 2, { 0, 3, 0, 0, 0 } },  // Urban Muscle
    { 2656397251u, 2, { 0, 3, 0, 0, 0 } },  // The Pretenders
    { 2897143988u, 3, { 0, 1, 4, 0, 0 } },  // Time Hunter
    { 3115208195u, 3, { 0, 1, 4, 0, 0 } },  // Midnight Rush
    { 3059479096u, 3, { 0, 1, 2, 0, 0 } },  // Don?t Drive Junk
    { 1763627507u, 4, { 0, 1, 3, 4, 0 } },  // Mountain Man
    {  156658199u, 4, { 0, 1, 3, 4, 0 } },  // Offroad Mayhem
    { 3835123886u, 3, { 0, 3, 4, 0, 0 } },  // Ford Challenge
    { 1072480740u, 3, { 0, 1, 4, 0, 0 } },  // Tioga Run
    { 2134719641u, 3, { 0, 1, 4, 0, 0 } },  // King Of The Road
    { 2344888867u, 3, { 0, 3, 4, 0, 0 } },  // Dry Heat
    {  119455665u, 3, { 0, 3, 4, 0, 0 } },  // Outrun
    { 1490425049u, 4, { 0, 3, 4, 5, 0 } },  // Vegas Velocity
    { 2190611244u, 4, { 0, 3, 4, 5, 0 } },  // Sin City Street Race
    { 1059026347u, 4, { 0, 1, 3, 4, 0 } },  // Lotus Battle
    { 2234446290u, 3, { 0, 3, 4, 0, 0 } },  // Feel The Heat
    { 3686337397u, 4, { 0, 1, 3, 4, 0 } },  // Hell On Wheels
    { 3927168791u, 4, { 0, 1, 3, 4, 0 } },  // The Edge
    {  969960617u, 3, { 0, 3, 4, 0, 0 } },  // Rally Challenge
    { 2629200658u, 3, { 0, 3, 4, 0, 0 } },  // Dust In My Eye
    { 1794521110u, 4, { 0, 1, 3, 4, 0 } },  // Snow Battle
    { 3749759140u, 4, { 0, 1, 2, 4, 0 } },  // Ascension
    { 4167843956u, 4, { 0, 1, 3, 4, 0 } },  // Double Black Diamond
    { 4259316566u, 4, { 0, 1, 3, 4, 0 } },  // Black Mountain
    { 1504994108u, 4, { 0, 1, 2, 4, 0 } },  // Just Drive
    { 3465724553u, 4, { 0, 1, 3, 4, 0 } },  // Dirty Drifter
    { 1691124658u, 5, { 0, 1, 2, 3, 4 } },  // Road Runner
    {  320218905u, 5, { 0, 1, 2, 3, 4 } },  // The Gap
    {  692073587u, 4, { 0, 1, 3, 4, 0 } },  // NFS Edition Showdown
    { 3095777409u, 4, { 0, 1, 3, 4, 0 } },  // Locals Only
    { 2722275898u, 3, { 0, 3, 4, 0, 0 } },  // Hard Action
    { 1358304694u, 3, { 0, 3, 4, 0, 0 } },  // Oncoming
    { 3354619962u, 2, { 0, 4, 0, 0, 0 } },  // Lower the Wacker
    { 1382978203u, 3, { 0, 3, 4, 0, 0 } },  // GT-R SpecV Showdown
    {  386720912u, 2, { 0, 4, 0, 0, 0 } },  // Street Legal
    { 1190250175u, 3, { 0, 3, 4, 0, 0 } },  // Lakeshore Battle
    { 1895859015u, 3, { 0, 3, 4, 0, 0 } },  // Interstate Loop
    { 2833054987u, 3, { 0, 4, 5, 0, 0 } },  // Industrial Run
    {  906430709u, 3, { 0, 4, 5, 0, 0 } },  // Tunnels and Trains
    {  119691382u, 3, { 0, 4, 5, 0, 0 } },  // Industrial Strength
    {  665564811u, 3, { 0, 3, 4, 0, 0 } },  // Hazard County
    { 2475630668u, 3, { 0, 3, 4, 0, 0 } },  // Traffic Jam
    { 3649528539u, 4, { 0, 1, 2, 4, 0 } },  // Porsche Attack
    { 3108061524u, 4, { 0, 1, 2, 4, 0 } },  // Lost Serpent Pass
    { 2208519834u, 4, { 0, 1, 2, 4, 0 } },  // Panic Attack
    { 1017959682u, 2, { 0, 4, 0, 0, 0 } },  // Drive Harder
    { 3368865327u, 4, { 0, 1, 2, 4, 0 } },  // The Jersey Double Back
    {  511196027u, 4, { 0, 1, 2, 4, 0 } },  // The Situation
    { 3643726335u, 2, { 0, 4, 0, 0, 0 } },  // Drive Hard
    { 3922471529u, 2, { 3, 4, 0, 0, 0 } },  // Drive Hard With A Vengeance
    { 3743660249u, 2, { 3, 4, 0, 0, 0 } },  // Live Free Or Drive Hard
    { 2673324721u, 2, { 0, 4, 0, 0, 0 } },  // The Final Event
    { 3867126210u, 2, { 0, 3, 0, 0, 0 } },  // Street Of San Francisco
    { 3952990312u, 3, { 0, 4, 5, 0, 0 } },  // The Gauntlet
    { 2403057324u, 2, { 0, 4, 0, 0, 0 } },  // Raw Power
    { 1090349791u, 3, { 0, 1, 4, 0, 0 } },  // Filtered For Success
    { 2203963555u, 3, { 0, 3, 4, 0, 0 } },  // Death Valley Run
    { 2749846213u, 3, { 0, 3, 4, 0, 0 } },  // Battle In The Desert
    { 3174878300u, 3, { 0, 3, 4, 0, 0 } },  // Make Tracks
    {  232080061u, 4, { 0, 1, 3, 4, 0 } },  // The Duel
    { 3016250220u, 3, { 0, 3, 4, 0, 0 } },  // Porsche Run
    {  383221778u, 2, { 0, 3, 0, 0, 0 } },  // San Francisco Challenge
    { 1014864742u, 3, { 0, 1, 4, 0, 0 } },  // Supercar Descent
    { 2187322303u, 2, { 0, 3, 0, 0, 0 } },  // Race of Races
    { 1615672593u, 4, { 0, 3, 4, 5, 0 } },  // Live The Underground
    { 1088884998u, 4, { 0, 1, 2, 4, 0 } },  // Fast Eddie
    { 1348262140u, 5, { 0, 1, 2, 3, 4 } },  // The Return Of Cross
    { 3882353303u, 4, { 0, 1, 3, 4, 0 } },  // Very Cross
    { 2869384708u, 4, { 0, 1, 3, 4, 0 } },  // Cross Strikes Back
    { 1419972188u, 3, { 0, 1, 4, 0, 0 } },  // The Most Wanted
    { 1442891702u, 4, { 0, 1, 3, 4, 0 } },  // Unstoppable Force
    { 1385274441u, 3, { 0, 3, 4, 0, 0 } },  // Razor?s Edge
    { 1762790502u, 4, { 0, 1, 2, 4, 0 } },  // I?m On A Horse
    {  801442788u, 3, { 0, 3, 4, 0, 0 } },  // Champion
    { 3174481921u, 3, { 0, 3, 4, 0, 0 } },  // Swagger
    { 2223360019u, 3, { 0, 3, 4, 0, 0 } },  // Tunnel of Pain
    { 3127775154u, 2, { 0, 3, 0, 0, 0 } },  // **CUT**
    { 1170798514u, 3, { 0, 1, 4, 0, 0 } },  // Altamont Pass
    {  748579042u, 3, { 0, 1, 4, 0, 0 } },  // Altamont Pass Reverse
    {  996230524u, 4, { 0, 1, 3, 4, 0 } },  // Yosemite Approach A
    { 2454337956u, 4, { 0, 1, 3, 4, 0 } },  // Yosemite Approach Reverse B
    { 4101321974u, 3, { 0, 3, 4, 0, 0 } },  // Yosemite South A
    { 1599632309u, 3, { 0, 3, 4, 0, 0 } },  // Yosemite South B
    {  536243661u, 3, { 0, 1, 4, 0, 0 } },  // Sierra Pass B
    { 1836275362u, 3, { 0, 3, 4, 0, 0 } },  // Desert Hills
    { 3640908374u, 3, { 0, 3, 4, 0, 0 } },  // Death Valley A
    { 3308825217u, 3, { 0, 3, 4, 0, 0 } },  // Death Valley B
    { 2494877324u, 4, { 0, 3, 4, 5, 0 } },  // Las Vegas East A
    {  187022175u, 4, { 0, 3, 4, 5, 0 } },  // Las Vegas East B
    { 2786791931u, 4, { 0, 3, 4, 5, 0 } },  // Las Vegas East Mini
    { 2567984127u, 3, { 0, 3, 4, 0, 0 } },  // Desert Hills Rev A
    { 3720932394u, 3, { 0, 3, 4, 0, 0 } },  // Death Valley Reverse
    { 1086584321u, 3, { 3, 4, 7, 0, 0 } },  // Desert Hills Rev B
    { 3533805161u, 4, { 0, 1, 3, 4, 0 } },  // Million Dollar HWY A
    { 2569133557u, 4, { 0, 1, 3, 4, 0 } },  // Million Dollar HWY B
    { 1979857418u, 4, { 0, 1, 3, 4, 0 } },  // Mountain Interstate Rev
    {  445415868u, 4, { 0, 1, 2, 4, 0 } },  // Independence Pass B
    { 1581736881u, 4, { 0, 1, 3, 4, 0 } },  // Montain Interstate
    { 3335861631u, 4, { 0, 1, 2, 4, 0 } },  // Independence Pass A
    { 2561852078u, 4, { 0, 1, 2, 4, 0 } },  // Indepenence Pass C
    { 2228323699u, 4, { 0, 1, 2, 4, 0 } },  // New Jersey Ind Rev
    {  148619112u, 4, { 0, 1, 3, 4, 0 } },  // Plains Interstate
    { 3247358286u, 5, { 0, 1, 2, 3, 4 } },  // Buffalo Gap
    {   95062145u, 5, { 0, 1, 2, 3, 4 } },  // Buffalo Gap Rev
    { 2678784262u, 4, { 0, 1, 3, 4, 0 } },  // Plains Interstate Rev
    { 3432517482u, 3, { 0, 3, 4, 0, 0 } },  // Rural Farms
    { 2614530869u, 3, { 0, 3, 4, 0, 0 } },  // Rural Highway
    { 1005996079u, 3, { 0, 3, 4, 0, 0 } },  // Chicago Interstate
    { 1130647398u, 2, { 0, 4, 0, 0, 0 } },  // Chicago Downtown Mini
    { 1880322147u, 2, { 0, 4, 0, 0, 0 } },  // Roof to Alley
    { 2641126162u, 2, { 0, 4, 0, 0, 0 } },  // Chicago Downtown Heli Chase
    {  250901999u, 2, { 0, 4, 0, 0, 0 } },  // Trapped with Train
    { 2024388306u, 2, { 0, 4, 0, 0, 0 } },  // Chicago Downtown After Car Select
    { 1371379625u, 4, { 0, 1, 3, 4, 0 } },  // Chicago Lake Shore B
    { 1924973096u, 3, { 0, 3, 4, 0, 0 } },  // Chicago Interstate Rev
    { 2740223046u, 3, { 0, 4, 5, 0, 0 } },  // Chicago Lake Shore A
    {  571722006u, 3, { 0, 4, 5, 0, 0 } },  // Chicago Industrial Action Level
    { 3592946720u, 3, { 0, 3, 4, 0, 0 } },  // Rural Farms Rev
    { 3308043497u, 3, { 0, 3, 4, 0, 0 } },  // Rural Highway Rev
    { 3504851546u, 4, { 0, 1, 2, 4, 0 } },  // Smoky Mountain Rev A
    {  601421103u, 4, { 0, 1, 2, 4, 0 } },  // Smoky Mountain Rev B
    { 2872627479u, 4, { 0, 1, 2, 4, 0 } },  // Smoky Mountain A
    { 3488452864u, 3, { 0, 4, 5, 0, 0 } },  // Chicago Interstate
    { 1080972529u, 4, { 0, 1, 2, 4, 0 } },  // New Jersey
    { 3720273376u, 4, { 0, 1, 2, 8, 0 } },  // New Jersey Rev
    { 3732571019u, 2, { 0, 4, 0, 0, 0 } },  // New York A
    { 1177725796u, 2, { 0, 4, 0, 0, 0 } },  // New York B
};

static const size_t kTodTableCount = sizeof(kTodTable) / sizeof(kTodTable[0]);

#endif // TOD_TABLE_H
