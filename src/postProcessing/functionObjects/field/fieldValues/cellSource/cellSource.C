/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | foam-extend: Open Source CFD
   \\    /   O peration     | Version:     4.0
    \\  /    A nd           | Web:         http://www.foam-extend.org
     \\/     M anipulation  | For copyright notice see file Copyright
-------------------------------------------------------------------------------
License
    This file is part of foam-extend.

    foam-extend is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    foam-extend is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with foam-extend.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "cellSource.H"
#include "fvMesh.H"
#include "volFields.H"
#include "IOList.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    namespace fieldValues
    {
        defineTypeNameAndDebug(cellSource, 0);
    }

    template<>
    const char* NamedEnum<fieldValues::cellSource::sourceType, 1>::
        names[] = {"cellZone"};

    const NamedEnum<fieldValues::cellSource::sourceType, 1>
        fieldValues::cellSource::sourceTypeNames_;

    template<>
    const char* NamedEnum<fieldValues::cellSource::operationType, 7>::
        names[] =
        {
            "none", "sum", "volAverage",
            "volIntegrate", "weightedAverage", "min", "max"
        };

    const NamedEnum<fieldValues::cellSource::operationType, 7>
        fieldValues::cellSource::operationTypeNames_;

}


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

void Foam::fieldValues::cellSource::setCellZoneCells()
{
    label zoneId = mesh().cellZones().findZoneID(sourceName_);

    if (zoneId < 0)
    {
        FatalErrorIn("cellSource::cellSource::setCellZoneCells()")
            << "Unknown cell zone name: " << sourceName_
            << ". Valid cell zones are: " << mesh().cellZones().names()
            << nl << exit(FatalError);
    }

    const cellZone& cZone = mesh().cellZones()[zoneId];

    cellId_.setSize(cZone.size());

    label count = 0;
    forAll(cZone, i)
    {
        label cellI = cZone[i];
        cellId_[count] = cellI;
        count++;
    }

    cellId_.setSize(count);
    nCells_ = returnReduce(cellId_.size(), sumOp<label>());

    if (debug)
    {
        Pout<< "Original cell zone size = " << cZone.size()
            << ", new size = " << count << endl;
    }
}


// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //

void Foam::fieldValues::cellSource::initialise(const dictionary& dict)
{
    switch (source_)
    {
        case stCellZone:
        {
            setCellZoneCells();
            break;
        }
        default:
        {
            FatalErrorIn("cellSource::initialise()")
                << "Unknown source type. Valid source types are:"
                << sourceTypeNames_ << nl << exit(FatalError);
        }
    }

    Info<< type() << " " << name_ << ":" << nl
        << "    total cells  = " << nCells_ << nl
        << "    total volume = " << gSum(filterField(mesh().V()))
        << nl << endl;

    if (operation_ == opWeightedAverage)
    {
        dict.lookup("weightField") >> weightFieldName_;
        if
        (
            obr().foundObject<volScalarField>(weightFieldName_)
        )
        {
            Info<< "    weight field = " << weightFieldName_;
        }
        else
        {
            FatalErrorIn("cellSource::initialise()")
                << type() << " " << name_ << ": "
                << sourceTypeNames_[source_] << "(" << sourceName_ << "):"
                << nl << "    Weight field " << weightFieldName_
                << " must be a " << volScalarField::typeName
                << nl << exit(FatalError);
        }
    }

    Info<< nl << endl;
}


void Foam::fieldValues::cellSource::writeFileHeader()
{
    if (outputFilePtr_.valid())
    {
        outputFilePtr_()
            << "# Source : " << sourceTypeNames_[source_] << " "
            << sourceName_ <<  nl << "# Cells  : " << nCells_ << nl
            << "# Time" << tab << "sum(V)";

        forAll(fields_, i)
        {
            outputFilePtr_()
                << tab << operationTypeNames_[operation_]
                << "(" << fields_[i] << ")";
        }

        outputFilePtr_() << endl;
    }
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::fieldValues::cellSource::cellSource
(
    const word& name,
    const objectRegistry& obr,
    const dictionary& dict,
    const bool loadFromFiles
)
:
    fieldValue(name, obr, dict, loadFromFiles),
    source_(sourceTypeNames_.read(dict.lookup("source"))),
    operation_(operationTypeNames_.read(dict.lookup("operation"))),
    nCells_(0),
    cellId_()
{
    read(dict);
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::fieldValues::cellSource::~cellSource()
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::fieldValues::cellSource::read(const dictionary& dict)
{
    fieldValue::read(dict);

    if (active_)
    {
        // no additional info to read
        initialise(dict);
    }
}


void Foam::fieldValues::cellSource::write()
{
    fieldValue::write();

    if (active_)
    {
        if (Pstream::master())
        {
            outputFilePtr_()
                << obr_.time().value() << tab
                << sum(filterField(mesh().V()));
        }

        forAll(fields_, i)
        {
            writeValues<scalar>(fields_[i]);
            writeValues<vector>(fields_[i]);
            writeValues<sphericalTensor>(fields_[i]);
            writeValues<symmTensor>(fields_[i]);
            writeValues<tensor>(fields_[i]);
        }

        if (Pstream::master())
        {
            outputFilePtr_()<< endl;
        }

        if (log_)
        {
            Info<< endl;
        }
    }
}


// ************************************************************************* //

