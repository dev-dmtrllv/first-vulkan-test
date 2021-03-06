Engine.onLoad(async (configure) => 
{
	Engine.log("Configuring engine...");
	
	await configure({
		name: "Nova Test Game",
		window: {
			minWidth: 640,
			minHeight: 480,
			resizable: true,
			maximized: true,
			width: 1920,
			height: 1080,
			hidden: true
		},
	});

	Engine.log("Engine configured!");

	Engine.window.show();
	
	Engine.start();
});

