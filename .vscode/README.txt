/*
 *		____					____
 *  / ____|           / ____|                
 * | (___   ___ _ __ | (___  _   _ _ __ ___  
 *  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
 *  ____) |  __/ | | |____) | |_| | | | | | |
 * |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
 * 
 * 
 * Description : A guide to link libraries to sendeca-firmware project
 * Created by  : James Le
 * Date        : 12/08/2020
 * 
 */
 
After clone a project from master repo, drop .vscode folder inside sendeca-firmware folder.
 
Dependency Graph
|-- <sendeca-firmware>
|   |-- <.git>
|   |-- <.vscode> 	<= drop this folder here
|   |-- <metaScripts>
|   |-- <Project>

Open VSCode, navigate to sendeca-firmware folder and then open folders inside. This will help VSCode
detects libraries nedded for the project.