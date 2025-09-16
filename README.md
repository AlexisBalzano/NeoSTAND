# NeoSTAND
Automatic Stand assignement for *NeoRadar* ATC Client <br>

# Installation
- Place `.nrplugin` inside `Documents/NeoRadar/Plugins` directory.
- Download airport config files and place them in `Documents/NeoRadar/Plugins/NeoSTAND` directory.
- Configure you `label.json` to show Stand TAG item.

# Usage
Stand will be assigned to arriving aircraft that are closer than **maxDistance** & lower than **maxALtitude** _(Configurable inside `config.json`)_.

# Commands
- `.stand help`: display all plugin available commands <br>
- `.stand version`: display loaded plugin version <br>
- `.stand airports`: display list of active airports <br>
- `.stand occupied`: display list of occupieds stands <br>
- `.stand blocked`: display list of blocked stands _(proximity to occupied stand)_ <br>
