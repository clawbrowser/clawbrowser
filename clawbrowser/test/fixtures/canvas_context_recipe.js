function render(canvas, frequent, seed, capture) {
    canvas.width=192; canvas.height=128;
    const c=canvas.getContext('2d', {willReadFrequently:frequent});
    const gradient=c.createLinearGradient(.25,.75,180.5,110.25);
    gradient.addColorStop(0,'#183654'); gradient.addColorStop(1,'#dbaf67');
    c.fillStyle=gradient; c.fillRect(0,0,192,128);
    c.save(); c.translate(11.125,9.375); c.rotate(.137);
    c.fillStyle='rgba(210,32,89,.71)';
    c.beginPath(); c.moveTo(3.2,11.7);
    c.bezierCurveTo(97.2,-20.8,154.9,130.1,171.4,68.6);
    c.lineTo(18.8,92.2); c.closePath(); c.fill(); c.restore();
    c.globalCompositeOperation='multiply';
    c.font='17px Arimo'; c.fillStyle='#395ac7';
    c.fillText('Latin 0123 Привет',3.25,43.75);
    c.font='16px sans-serif';
    c.fillText('العربية 漢字 🧭',4.5,78.25);
    c.globalCompositeOperation='source-over';
    c.shadowColor='rgba(20,90,180,.4)'; c.shadowBlur=3.5;
    c.strokeStyle='#65deab'; c.lineWidth=1.25;
    c.beginPath(); c.ellipse(91.25,86.75,33.5,17.25,.23,0,Math.PI*2); c.stroke();
    if(capture)capture(-1,Array.from(c.getImageData(0,0,192,128).data));
    let state=seed;
    const random=()=>((state=(Math.imul(state,1664525)+1013904223)>>>0)/4294967296);
    const color=()=>`rgba(${Math.floor(random()*256)},${Math.floor(random()*256)},${Math.floor(random()*256)},${.2+random()*.8})`;
    for(let i=0;seed && i<16;i++) {
        c.save();
        c.globalCompositeOperation=['source-over','multiply','screen','xor'][i%4];
        c.shadowBlur=random()*7; c.shadowColor=color();
        const g=c.createRadialGradient(random()*192,random()*128,0,96,64,40+random()*120);
        g.addColorStop(0,color()); g.addColorStop(.37,color()); g.addColorStop(1,color());
        c.fillStyle=g;
        c.beginPath(); c.moveTo(random()*192,random()*128);
        c.bezierCurveTo(random()*240-24,random()*180-26,
            random()*240-24,random()*180-26,random()*192,random()*128);
        c.lineTo(random()*192,random()*128); c.closePath(); c.fill();
        c.restore();
        if(capture)capture(i,Array.from(c.getImageData(0,0,192,128).data));
    }
    return Array.from(c.getImageData(0,0,192,128).data);
}
