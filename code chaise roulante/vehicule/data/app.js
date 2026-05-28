let holdInterval = null;
let currentHoldButton = null;


// ==================== REQUETES ====================

function sendRequest(path) {

    return fetch(path)
        .catch(function () {

            setConnection(false);

        });

}

function sendCommand(action) {

    sendRequest(
        "/cmd?action=" + action
    );

    addLog(
        "Commande : " +
        action.toUpperCase()
    );

}


// ==================== MODES ====================

function setMode(mode) {

    sendRequest(
        "/mode?value=" + mode
    );

    updateActiveMode(mode);

    addLog(
        "Mode " +
        mode.toUpperCase() +
        " activé"
    );

}

function updateActiveMode(mode) {

    const modes = {

        ir:
            document.getElementById(
                "btnModeIr"
            ),

        web:
            document.getElementById(
                "btnModeWeb"
            ),

        voice:
            document.getElementById(
                "btnModeVoice"
            )

    };

    Object.values(modes)
        .forEach(function (button) {

            button.classList.remove(
                "active-mode"
            );

        });

    if (modes[mode]) {

        modes[mode]
            .classList.add(
                "active-mode"
            );

    }

    document
        .getElementById(
            "modeActif"
        )
        .innerText =
        mode.toUpperCase();

    document
        .getElementById(
            "footerState"
        )
        .innerText =
        "Mode " +
        mode.toUpperCase() +
        " actif";

}


// ==================== MAINTIEN ====================

function startHoldCommand(action, button) {

    if (holdInterval !== null) {
        clearInterval(holdInterval);
        holdInterval = null;
    }

    currentHoldButton = button;

    button.classList.add("pressed");

    sendCommand(action);

    holdInterval = setInterval(function () {
        sendCommand(action);
    }, 120);

}

function stopHoldCommand(button) {

    if (holdInterval !== null) {
        clearInterval(holdInterval);
        holdInterval = null;
    }

    if (currentHoldButton) {
        currentHoldButton.classList.remove("pressed");
        currentHoldButton = null;
    }

    button.classList.remove("pressed");

    sendCommand("stop");

}

function setupHoldButtons() {

    const buttons = document.querySelectorAll(".drive-btn");

    buttons.forEach(function (button) {

        const action = button.dataset.action;

        button.addEventListener("pointerdown", function (event) {
            event.preventDefault();
            startHoldCommand(action, button);
        });

        button.addEventListener("pointerup", function (event) {
            event.preventDefault();
            stopHoldCommand(button);
        });

        button.addEventListener("pointercancel", function (event) {
            event.preventDefault();
            stopHoldCommand(button);
        });

        button.addEventListener("pointerleave", function (event) {
            event.preventDefault();
            stopHoldCommand(button);
        });

    });

}


// ==================== FEUX BOUTONS ====================

function updateOutputs(data) {

    const avantBtn =
        document.getElementById(
            "btnFeuxAvant"
        );

    const arriereBtn =
        document.getElementById(
            "btnFeuxRouges"
        );


    avantBtn.classList.toggle(
        "active-output",
        data.ledAvant
    );

    arriereBtn.classList.toggle(
        "active-output",
        data.feuxRouges
    );


    document
        .getElementById(
            "frontIndicator"
        )
        .classList.toggle(
            "on",
            data.ledAvant
        );


    document
        .getElementById(
            "rearIndicator"
        )
        .classList.toggle(
            "on",
            data.feuxRouges
        );


    document
        .getElementById(
            "frontState"
        )
        .innerText =

        data.ledAvant
            ? "ON"
            : "OFF";


    document
        .getElementById(
            "rearState"
        )
        .innerText =

        data.feuxRouges
            ? "ON"
            : "OFF";

}


// ==================== VEHICULE ====================

function updateVehicle(data) {

    // Avant

    document
        .getElementById(
            "frontLeft"
        )
        .classList.toggle(
            "front-on",
            data.ledAvant
        );

    document
        .getElementById(
            "frontRight"
        )
        .classList.toggle(
            "front-on",
            data.ledAvant
        );


    // Arrière

    document
        .getElementById(
            "rearLeft"
        )
        .classList.toggle(
            "rear-on",
            data.feuxRouges
        );

    document
        .getElementById(
            "rearRight"
        )
        .classList.toggle(
            "rear-on",
            data.feuxRouges
        );


    // Clignotant gauche

    document
        .getElementById(
            "yellowLeft"
        )
        .classList.toggle(
            "blink",
            data.clignotantGauche
        );


    // Clignotant droite

    document
        .getElementById(
            "yellowRight"
        )
        .classList.toggle(
            "blink",
            data.clignotantDroite
        );


    // Rouge arrière gauche

    document
        .getElementById(
            "rearLeft"
        )
        .classList.toggle(
            "blink",
            data.clignotantGauche
        );


    // Rouge arrière droite

    document
        .getElementById(
            "rearRight"
        )
        .classList.toggle(
            "blink",
            data.clignotantDroite
        );

}


// ==================== ALERTES ====================

function updateAlerts(data) {

    document
        .getElementById(
            "alerteAvant"
        )
        .innerText =
        data.alerteAvant;


    document
        .getElementById(
            "alerteArriere"
        )
        .innerText =
        data.alerteArriere;


    document
        .getElementById(
            "alarmeMuette"
        )
        .innerText =
        data.alarmeMuette;

}


// ==================== CONNEXION ====================

function setConnection(ok) {

    document
        .getElementById(
            "connexionEtat"
        )
        .innerText =

        ok

            ? "OK"

            : "OFF";

}


// ==================== JOURNAL ====================

function addLog(message) {

    const log =

        document
            .getElementById(
                "logText"
            );


    const heure =

        new Date()
            .toLocaleTimeString();


    log.innerText =

        "[" +

        heure +

        "] " +

        message;

}


// ==================== ETAT ESP32 ====================

function refreshState() {

    fetch("/etat")

        .then(function (response) {

            return response.json();

        })

        .then(function (data) {

            setConnection(
                true
            );

            updateActiveMode(
                data.mode
            );

            updateOutputs(
                data
            );

            updateVehicle(
                data
            );

            updateAlerts(
                data
            );

        })

        .catch(function () {

            setConnection(
                false
            );

        });

}


// ==================== INITIALISATION ====================

setupHoldButtons();

updateActiveMode(
    "ir"
);

setInterval(

    refreshState,

    500

);

refreshState();
