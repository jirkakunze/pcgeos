#####################################################################
# 
# PROJECT:          ShiSen-Sho Game for PC/Geos
#
# DESCRIPTION:	    a japanese game similar to mahjongg
#
# FILE:             shisen.gp
#
# REMARK:           Licensed under the Apache License, Version 2.0
#                   (the "License"); you may not use this file
#                   except in compliance with the License.
#                   You may obtain a copy of the License at
#
#                       http://www.apache.org/licenses/LICENSE-2.0
#
#                   Unless required by applicable law or agreed to
#                   in writing, software distributed under the
#                   License is distributed on an "AS IS" BASIS,
#                   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
#                   either express or implied. See the License for the
#                   specific language governing permissions and
#                   limitations under the License.
#
# REVISION HISTORY:
#       Date      Name      Description
#       ----      ----      -----------
#       08.08.00  JK        initial version
#       14.11.25  JK        clean up
#
#####################################################################

name shisen.app

longname "Shisen-Sho"

type	appl, process, single

class	ShisenProcessClass

appobj  ShisenApp

tokenchars	"ShiS"
tokenid		0

library	geos
library	ui
library game
library ansic

resource AppResource	ui-object
resource Interface	ui-object
resource BoardResource	ui-object
resource CrakTiles  	ui-object
resource DotTiles   	ui-object
resource BambooTiles	ui-object
resource SeasonTiles	ui-object
resource DirectionTiles ui-object
resource FlowerTiles	ui-object
resource TextStrings    lmem data

export ShisenProcessClass
export ShisenBoardClass
