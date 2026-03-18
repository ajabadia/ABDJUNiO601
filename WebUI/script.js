// ... [inside window.onload] ...
    callNative("uiReady");
    
    // Smooth fade-in once we are synchronized
    setTimeout(() => {
        document.getElementById('synth-app').style.opacity = '1';
        document.body.style.color = '#fff'; // Restore color
    }, 100);
});
// ... [rest of file unchanged] ...
