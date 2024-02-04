name shisen.app

longname "Shisen-Sho"

type	appl, process, single

class	ShisenProcessClass

appobj ShisenApp

tokenchars	"ShiS"
tokenid		0

library	geos
library	ui
library game
library ansic

platform geos20

resource AppResource	ui-object
resource Interface	    ui-object
resource BoardResource	ui-object
resource CrakTiles  	ui-object
resource DotTiles   	ui-object
resource BambooTiles	ui-object
resource SeasonTiles	ui-object
resource DirectionTiles ui-object
resource FlowerTiles	ui-object

export ShisenProcessClass
export ShisenBoardClass
