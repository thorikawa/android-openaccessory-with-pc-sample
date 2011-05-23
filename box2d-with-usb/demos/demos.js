var initId = 0;
var world = createWorld();
var ctx;
var canvasWidth;
var canvasHeight;
var canvasTop;
var canvasLeft;
var unit = 0.005;
var eps = 0.02;

function setupWorld(did) {
	if (!did) did = 0;
	world = createWorld();
	initId += did;
	initId %= demos.InitWorlds.length;
	if (initId < 0) initId = demos.InitWorlds.length + initId;
	demos.InitWorlds[initId](world);
}
function setupNextWorld() { setupWorld(1); }
function setupPrevWorld() { setupWorld(-1); }
function step(cnt) {
	var angle;
	new Ajax.Request("angle.txt",
		{ method: 'get',
		  asynchronous:false,
		  onComplete: function (httpObj) {
		    angle = httpObj.responseText;
		  }
		}
	);
	var balln;
	new Ajax.Request("ball.txt",
		{ method: 'get',
		  asynchronous:false,
		  onComplete: function (httpObj) {
		    balln = parseInt(httpObj.responseText);
		  }
		}
	);
	if (demos.balln < balln) {
		demos.top.createBall(world, Math.random()*450+25, Math.random()*100);
		demos.balln++;
	}
	angle = 3.14*2*parseInt(angle)/360;
	if (demos.angle > angle + eps) {
		demos.angle -= unit;
	} else if (demos.angle < angle - eps) {
		demos.angle += unit;
	}
	demos.ground.SetCenterPosition(new b2Vec2(250, 300), demos.angle);
	var stepping = false;
	var timeStep = 1.0/60;
	var iteration = 1;
	world.Step(timeStep, iteration);
	ctx.clearRect(0, 0, canvasWidth, canvasHeight);
	drawWorld(world, ctx);
	setTimeout('step(' + (cnt || 0) + ')', 10);
}
Event.observe(window, 'load', function() {
	setupWorld();
	ctx = $('canvas').getContext('2d');
	var canvasElm = $('canvas');
	canvasWidth = parseInt(canvasElm.width);
	canvasHeight = parseInt(canvasElm.height);
	canvasTop = parseInt(canvasElm.style.top);
	canvasLeft = parseInt(canvasElm.style.left);
	Event.observe('canvas', 'click', function(e) {
		demos.top.createBall(world, Event.pointerX(e) - canvasLeft, Event.pointerY(e) - canvasTop);
	});
	demos.angle = 0;
	new Ajax.Request("ball.txt",
		{ method: 'get',
		  asynchronous:false,
		  onComplete: function (httpObj) {
		    demos.balln = parseInt(httpObj.responseText);
		  }
		}
	);
	step();
});
